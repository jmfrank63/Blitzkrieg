---
phase: 03-map-editor-plan-6-finish-m1
plan: 15
subsystem: map-editor
tags: [zig, ci, exit-criteria, map-sweep, spec, release-build]

# Dependency graph
requires:
  - phase: 03-map-editor-plan-6-finish-m1 (plans 01-14)
    provides: "The whole M1 editor: test launch, safe save, settings/autosave/recovery, mods, palette pictures, sound list, BK_EDITOR_AUTO, packaging"
provides:
  - "M1 exit-criteria evidence: full sweep 1,755/1,755, local tiers 11 PASS lines, CI run 36473046568 green on all six jobs"
  - "The portable-map-editor spec corrected to the built M1 (test launch, -mod=Name, safe save, user data, camera decision, pictures, sound list, packaging, BK_EDITOR_AUTO, exit criteria)"
  - "A release-variant Game + MapEditor stage for Johannes's hand try"
affects: [/gsd-verify-work for phase 3, M2 planning]

actuals:
  tokens: 9200
  tasks: 3
  commits: 1
plan_head_before: 87a2028f0

tech-stack:
  added: []
  patterns:
    - "Exit criteria in the spec cite the command and date, not an ephemeral CI run number for this plan; the run id lives in the SUMMARY"

key-files:
  created: []
  modified:
    - docs/superpowers/specs/2026-09-19-portable-map-editor-design.md

key-decisions:
  - "Task 1 changed no source: every shipped map already round-trips, so MapFile.cpp and MapEquivalence.cpp were not touched"
  - "The spec cites CI by the 03-14 run for the build criterion and points here for this plan's own run, so the spec needs no edit after each CI run"
  - "Spec exit criterion 3 records that 'rotates units' means object rotation (Q/E); camera rotation is the separately deferred D-12"
  - "D-26 revised 2026-09-29 (Johannes, hand try step 7): switching mod closes the map (prompt first, then close, then switch); the active mod again is a no-op"

requirements-completed: [M1-EXIT-SWEEP, M1-EXIT-BUILD, M1-EXIT-TIERS]

coverage:
  - id: D1
    description: "Every shipped map (1,755 .bzm plus the .xml maps) opens and saves unchanged to an equivalent map"
    requirement: "M1-EXIT-SWEEP"
    verification:
      - kind: integration
        ref: "zig build test-map-files-all -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run (zig-out/local-test/03-15-t1-sweep.log: 1755 of 1755 round-tripped, map-file: PASS, 0 FAIL)"
        status: pass
    human_judgment: false
  - id: D2
    description: "Core and map-file tiers pass on all six CI targets and MapEditor builds on its two platforms"
    requirement: "M1-EXIT-BUILD"
    verification:
      - kind: e2e
        ref: "CI run 36473046568 at a8ee30d10: all six jobs success"
        status: pass
    human_judgment: false
  - id: D3
    description: "Engine, game-reads-it and editor-app tiers pass locally on macOS arm64"
    requirement: "M1-EXIT-TIERS"
    verification:
      - kind: integration
        ref: "zig build test; zig build test-editor-bridge test-map-editor-engine map-editor-host-check map-editor-smoke map-editor-game-reads-it map-editor-auto -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run (11 PASS/done lines, rc=0)"
        status: pass
    human_judgment: false
  - id: D4
    description: "The spec matches the software plan 6 built and decided"
    verification:
      - kind: other
        ref: "grep -c 'editor-test|mapeditor.cfg|BK_EDITOR_AUTO|-mod=' spec = 10 (threshold 4); commit a8ee30d10"
        status: pass
    human_judgment: true
    rationale: "Whether the prose says what Johannes decided is a reading judgment; the grep proves only that the named items are present."
  - id: D5
    description: "Johannes's hand try on macOS arm64 with the release build (M1-EXIT-HANDTRY)"
    requirement: "M1-EXIT-HANDTRY"
    verification: []
    human_judgment: true
    rationale: "Real trackpad, real window, real play; cannot be driven from the agent shell. PENDING: result not yet reported."

duration: ~95min (including ~27min unattended CI wait and a ~4min release build)
completed: 2026-09-29
status: complete
---

# Phase 3 Plan 15: M1 exit sweep, suite, CI, spec and hand-try hand-over Summary

**All automated M1 exit criteria are met with evidence (1,755/1,755 shipped maps round-trip, 11 local tier PASS lines, CI green on all six jobs), the spec now describes what plan 6 built and decided, and a release Game plus MapEditor stage is ready for Johannes's hand try, which is still pending.**

## Performance

- **Duration:** ~95 min, of which about 27 min was waiting for the Windows CI job and about 4 min was the release build
- **Completed:** 2026-09-29
- **Tasks:** 3 (Task 3's human check is pending, see below)
- **Files modified:** 1 (the spec)

## Accomplishments

- **Full sweep:** `zig build test-map-files-all` reported `map-file: sweeping 1755 maps`, `1755 of 1755 maps round-tripped`, `map-file: PASS`, zero `FAIL:` lines; 18 s wall time with a warm cache. No source changed.
- **Local suite:** `zig build test` rc=0 (`wheel scroll: PASS`). The six tiers ran in 2 min 49 s, rc=0, with these lines:
  - `map-editor: host check PASS (metal, 1280x800)` (three runs, one with `-mod=EditorTestMod`) and three `panel smoke PASS (5559 catalogue entries, 184 tiles, ...)`
  - `map-editor: smoke PASS (30 steps, 260 objects, ...)`
  - `editor-bridge: PASS`
  - `map-editor: game reads it PASS (4 units of player 0 near the placed unit, game exit 0)`
  - `map-editor: BK_EDITOR_AUTO: done (13 actions)`
  - `map-editor-engine: PASS (260 objects)`
- **CI:** run 36473046568 (`workflow_dispatch` on `feat/map-editor-plan-6`, head `a8ee30d10`), all six jobs success:

  | Job | Result | Time |
  |---|---|---|
  | linux-platform | success | 6.3 min |
  | linux-arm-platform | success | 1.2 min |
  | macos-platform | success | 18.0 min |
  | macos-intel-platform | success | 2.7 min |
  | windows-mingw-platform | success | 7.8 min |
  | windows-platform | success | 26.8 min |

  Windows budget check (research Pitfall 6): 26.8 min of the 110-minute timeout, well inside it; the earlier 55-min run (36464351662) was a cold-cache run.
- **Spec:** `docs/superpowers/specs/2026-09-19-portable-map-editor-design.md` updated in `a8ee30d10`: M1 scope (camera rotation deferred with its 03-06 measurements, `-mod=Name`, sound list, Windows app), preservation invariant (sounds now edited), settings location, new sections "Object palette pictures", "Editor: sound list" and "User data: maps, settings and mods", packaging beside the game with the Windows GUI subsystem, the test launch as built (`-editor-test`, MapEditorTest generated-data root), safe save with `.bak`, BK_EDITOR_AUTO grammar and automated shot comparison, and each exit criterion marked with evidence.
- **Release stage:** `zig build install-map-editor -Dtarget=aarch64-macos --release=fast -Dcopy-data=false` rc=0; `Game` and `MapEditor` in `/Users/johannes/Projects/src/Blitzkrieg/.worktrees/map-editor-6/zig-out/game/macos/arm64/release/`. `MapEditor --check Data\Maps\Multiplayer\coldwinter.bzm` printed `host check PASS (metal, 1280x800)`, `unknown-object warning PASS`, `panel smoke PASS`; `Game -help` starts and exits 0. Disk after build: 13 GB free.

## Task Commits

1. **Task 1: full sweep** - no commit (no source change needed; log in `zig-out/local-test/03-15-t1-sweep.log`, gitignored)
2. **Task 2: whole suite locally and in CI, spec brought up to date** - `a8ee30d10` (docs)
3. **Task 3: release build for the hand try** - no commit (build artifacts are gitignored; spec hand-try line to be finalised when Johannes reports)

**Plan metadata:** the docs commit carrying this SUMMARY and STATE.md.

## Hand-try checklist (Task 3, PENDING for Johannes)

Build: `/Users/johannes/Projects/src/Blitzkrieg/.worktrees/map-editor-6/zig-out/game/macos/arm64/release/`. Start `MapEditor` from that folder (`cd` there, then `./MapEditor`). This is the worktree's own stage; his live installation and profile are not involved, and the test game uses the `MapEditorTest` profile.

1. Open `Data\Maps\Multiplayer\coldwinter.bzm`. Paint tiles, place a unit, move it, rotate it with Q/E, delete it, change a side in Players; undo and redo all of it.
2. Save: on a shipped map it becomes Save As into your maps folder; save.
3. F5: the game opens windowed beside the editor and plays the saved state with your unit. Quit the game: the editor still has the map and its history. F5 twice: the restart prompt.
4. Pinch and Shift+swipe zoom at the pointer, a plain swipe pans, Home resets zoom.
5. Close the window with changes: Save / Don't save / Cancel.
6. Edit > Settings: swipe speed changes the pan, autosave interval; File > Open Recent.
7. File > Mod with a mod you have installed locally (never committed).
8. Palette pictures in a few groups: each object shows its own icon; 36 objects show a neutral frame with their name. Single soldiers are no longer listed (see the lone soldier gap fix below), so place their squads instead. That removes 5 of the 41 neutral frames the bridge's own picture sweep counts: the paratroopers, `Australian_Shooter` and `Finnish_Gunner_Hw`, which are soldiers that no squad lists.
9. The Sounds panel on a map with sounds (none of the shipped maps has any, so add one at the view centre, edit it, undo and redo, save, reopen).
10. A shipped map opens read-only (title says read-only; Save asks for a new name).

Report "approved" or the failing numbers. Camera rotation is deliberately not on the list (deferred in 03-06).

Result: **pending**.

## Gap fix: a map in another installation's Data was saved in place (hand try, step 2)

**Found:** from the release stage, File > Open of `/Users/johannes/Projects/src/Blitzkrieg/Data/Maps/Multiplayer/coldwinter.bzm` (the main checkout's Data, not the stage's), then File > Save, wrote into that file and left `coldwinter.bzm.bak` beside it. D-18 says a shipped map is read-only and Save becomes Save As.

**Root cause:** `panels_logic.isShippedMap(engine_path, base_root)` compared text only: relative `data/...`, or an absolute path starting with the editor's own base root. Another installation's Data, or the stage's own Data reached through its symlink's target, looked like a user's file. Autosave (D-20) used the same answer, so it would have written into that file every 2 minutes as well.

**New rule** (`Sources/editor/core/shipped.zig`, std-only, the disk questions behind `Files.isDataRoot`/`Files.realPath`): a map is read-only when, for its path as given or its real path (symlinks resolved), any of these holds:
1. it is under `<base_root>Data/` or `<base_root>mods/<N>/data/`, checked against the base root and the base root's real path;
2. it is under `mods/<N>/data/` anywhere;
3. one of its ancestor folders is named `Data` (any case) and has a game data root marker at its top: `consts.xml`, `objects.xml`, `mod.xml`, `resource.description` or any `*.pak`.

A folder of your own that happens to be called `Data` stays writable because it has no marker. The user maps folder, `mods/<N>/maps`, the recovery folder and the generated test-map root have no `data` segment, so they stay writable.

**Where it applies:** Save turns into Save As, autosave writes only to the recovery copy (D-22), and the title says "(read-only)". The panels cache one answer per document path, so the disk is only asked again when the path changes. `Editor.save` also refuses any target inside a game's data before it writes anything, not even a temp file or a `.bak`. The status line then reads "not saved: <name> is inside a game's data folder, which is read-only - Save As into your maps folder instead". This catches any caller that forgets the rule.

**Commit:** `48bf7cfe8` fix(03-15): a map inside any game's Data is read-only, not only the editor's own.

**Evidence** (debug stage `zig-out/game/macos/arm64/debug`; the release stage was not touched because Johannes was running it):
- `test-editor-core` 85/85 passed, including 9 new `shipped.zig` tests. They cover own Data relative and absolute, another tree's Data, Windows drive-letter and backslash paths, a symlinked Data both ways, a symlinked base root, `mods/<N>/data`, the user maps and recovery folders, an unmarked `Data`, and a real-disk case with a real symlink. There is also a new `Editor.save` refusal test: nothing is written, copied, renamed or deleted.
- `test-map-editor-panels` 45/45 passed, including a new foreign-tree `needsSaveAs` test and an autosave-target test. `test-map-editor-auto` passed. `zig build test` passed 203/203.
- `map-editor-smoke` PASS, 35 steps. The 5 new steps build `zig-out/local-test/map-editor-smoke-foreign/Data` with a `consts.xml` marker and a copied `coldwinter.bzm`, and open it by its absolute path. The map shows as read-only. After a dirtying edit, Save becomes Save As. A due autosave writes only `map-editor-smoke-user/mapeditor/recovery/coldwinter.bzm`, because the smoke points its user root under local-test and never into the real one. A forced in-place `editor.save` returns `error.Refused`. After every step the foreign file's bytes (Wyhash), size, inode and mtime are unchanged, and there is no `.bak` and no `.~save` temp.
- Negative controls, reverted afterwards: with the panels' classifier given no `Files`, the smoke fails with "another installation's map opens read-only: ... is not read-only". With the `Editor.save` guard disabled, it fails with "Editor.save wrote into ..." and the new core test fails.
- `map-editor-host-check` passed with the relative path, the absolute path through the stage's Data symlink, and `-mod=EditorTestMod`.
- The core and panels tests also cross-compile for `x86_64-windows-gnu` (with `--test-no-exec`).

**Still to do:** rebuild the release stage (`zig build install-map-editor -Dtarget=aarch64-macos --release=fast -Dcopy-data=false`) and run its `MapEditor --check`. The orchestrator does this once Johannes has left the release folder. Then retry hand-try step 2 on the main checkout's `coldwinter.bzm`. When this was written, the main checkout's `Data/Maps/Multiplayer/coldwinter.bzm` matched git (`git status` was clean, mtime 04:34) and the first try's `.bak` was already gone. This fix only read that checkout and wrote nothing to it.

## Gap fix: season (hand try, M1)

**Found:** on the winter map `Data/Maps/Multiplayer/coldwinter.bzm` (`nSeason = 1`), every unit was drawn with its summer textures. That covered the map's own units and every newly placed one. For example, the 10.5-cm Flak38 was tan instead of its `1w_c.dds` grey, and the infantry wore summer uniforms.

**Root cause:** the bridge never told the world the map's season. `CWorldBase` starts on `SEASON_SUMMER` (`Common/WorldBase.cpp:166`). `CWorldBase::CreateMapObject` passes the world's `nSeason` to every `pMO->Create`, and each map object picks its model and texture once, when it is created: `MOUnitMechanical` picks `\1w`, `MOUnitInfantry` uses `GetNameWithSeason`, and `MOObject` and `MOBuilding` use `GetSeasonApp`. The static `CMOUnit::nSeason` is set from the same argument. The game calls `pWorld->SetSeason( mapinfo.nSeason )` after the terrain and before the mission's objects (`GameTT/iMissionInternal.cpp:1495`). The MFC editor calls it on every load (`MapEditor/TemplateEditorFrame1.cpp:1683`). `OpenMapIntoSession` did neither.

**Fix** (`Sources/src/EditorBridge/session.cpp`, `OpenMapIntoSession`): after `pScene->SetTerrain` and before `PlaceObjects`/`BuildBridges`/`UpdateSessionWorld`, the session calls `pWorld->SetSeason( working.nSeason )`, in the game's order. This also sets the scene's sun (`IScene::SetSeason`), the `World.Season` global and the season's flash colours. Every world rebuild goes through this function: `BkEditorOpenMap` is its only caller, and the app's open, reopen and reload all use it. A mod switch closes the map, and the app then reopens it through the same call. Undo and redo edit the world in place and never rebuild it, so objects they add use the season already set. The editor cannot edit the season. Opening a summer map after a winter one resets the season, so no map inherits another's.

**Commit:** `933f99bd4` fix(03-15): a winter map's units are drawn in their winter paint.

**Evidence** (debug stage `zig-out/game/macos/arm64/debug`; the release stage was not touched because Johannes was running it):
- New engine-tier test `TestSeasonPicksTheVisuals` in `tools/zig/editor_bridge_test.cpp`. It opens coldwinter and then arnheim (summer), so the second open has to undo the first's season. For each map it checks `summary.season` and `World.Season`. It then points the camera at the map's first 6 units and squads, places a `10.5-cm_Flak38` and a `German_rifle_39` squad on bare ground, and reads the texture each unit on screen is drawn with. It does this with an `ISceneVisitor` over `IScene::Pick(screen rect, SGVOGT_UNIT)` plus `ITextureManager::GetTextureName`, which is the key the renderer actually binds. Winter must be `<n>w`/`<n>bw` and summer `<n>`/`<n>b`.
- Before the fix (test written first; log `zig-out/local-test/season-BEFORE-fix.log`), on coldwinter:
  - `World.Season` was unset.
  - 0 of 67 unit pictures of the map's own units were winter (`t_34_85\1`, `js_3\1`, ...).
  - The placed Flak38 was 0/1 winter (`10_5_cm_flak38\1`) and the placed infantry 0/10 (`mauser\1`).
  - The Flak38 was drawn at a mean RGB of 90,89,67.
- After the fix (log `zig-out/local-test/season-AFTER-fix.log`):
  - coldwinter: the map's own units were 67/67 winter, the Flak38 1/1 and the infantry 10/10. The Flak38 was drawn at a mean RGB of 124,134,125, grey (its textures average 55,48,33 summer and 94,95,90 winter).
  - arnheim: 58/58 own, 1/1 Flak38 and 10/10 infantry stayed in summer textures.
  - `test-editor-bridge` PASS.
- Captures: `zig-out/local-test/season-winter-BEFORE-fix.png` and `season-winter-AFTER-fix.png` (640x480, also `.tga`). Side by side: `zig-out/local-test/season-winter-before-after-crop.png`, where the tan Flak38 becomes grey.
- `map-editor-smoke` PASS (35 steps, coldwinter). `map-editor-host-check` PASS (host check, unknown-object warning, panel smoke).
- `test-map-editor-engine`: 1 pass, 1 fail. The failure is `expectLoneSoldierRefused` at `Sources/editor/app/c_bridge_test.zig:87` (`placeable` expected 0, found 1). That code belongs to the lone-soldier gap fix another session had uncommitted in this worktree at the time (`catalogue.cpp`, `bridge.h`, `c_bridge_test.zig`), not to this change. The test's open, edit and command steps before that line ran with the season fix in. Re-run it once that work lands.

**Still to do:** rebuild the release stage once Johannes has left it, then look at coldwinter in the release MapEditor.

## Gap fix: lone soldier crash (hand try, F5)

**Found:** Johannes saved a map from coldwinter.bzm with units from the palette's SGVOGT_UNIT group. Among them was a lone sniper (`Us_Sniper`). F5 then crashed the test game during mission start:
- `EXC_BAD_ACCESS` at `0x1b8` in `CSoldierRestState::Segment()+88`, under `CAILogic::Init -> CAILogic::Segment`.
- Registers: `x0 = 0`, `x8 =` the `CSoldier::GetFormation` thunk.

The saved map, `~/.local/share/Nival/Blitzkrieg/maps/mytest.bzm`, is identical to the test copy the game loaded. It holds `Us_Sniper` as a unit record, next to `USSR_rpd_43` squad records.

**Root cause:** the map stores a soldier as a unit record, which gives it no formation, and every soldier state assumes it has one.
- The game builds a soldier's formation only from a squad record. `CAILogic::AddObject` hands an SGVOGT_SQUAD record to `CUnitCreation::AddNewFormation`. An SGVOGT_UNIT record goes to `AddNewUnit`, which makes a bare `CSoldier`/`CSniper` for infantry.
- `CSoldierRestState::Segment` calls `pUnit->GetFormation()->IsInWaitingState()` without a null check on the first AI segment, which runs inside `Init`. `Soldier.cpp` has 20+ more unchecked `GetFormation()->` calls, so a check at the crash site would only move the crash.
- The MFC editor never wrote such a record. Its palette (`TabSimpleObjectsDialog.cpp` `CommonFilterName`) drops every path containing `humans`, and all 59 soldier types are `units\Humans\...`. It also drops `aisingleunitformation`. It saves squads as squad records and writes unit records only for objects with no formation.
- None of the 1,753 shipped `.bzm` files holds a soldier record (a scan for exact-case soldier names).
- Our palette listed every SGVOGT_UNIT (`panels_logic.isPlaceable` filters only sounds and tank pits), and the bridge placed any of them.

**Links (checked, nothing to fix):** a map has no links from soldiers to their squad. A squad is one record, and the game builds its soldiers from the squad's stats. `link.nLinkWith` ties a squad or unit to the thing it is inside or towing.
- Move changes only position, direction and player.
- Delete is refused while anything links to the object.
- Undo of a delete puts the saved record back unchanged: same link ID, same place in its list.
- Add writes `nLinkWith = -1`.

**Fix, editor side (valid map output, as the MFC editor):**
- `BkEditorAddObject` refuses a single soldier. A single soldier is what `SGDBObjectDesc::IsHuman` reports: an infantry SGVOGT_UNIT, drawn as a sprite, where every vehicle and gun is a mesh.
- The refusal message names a squad to place instead. It picks a squad of that soldier alone where the database has one (`Us_Sniper` -> `US_sniper`). Otherwise it picks the alphabetically first squad that lists the soldier (`Allies_Bren` -> `GB_bren_43`). It never suggests `AISingleUnitFormation`.
- `BkEditorCatalogueEntry` gains `placeable`: 0 for sounds, tank pits and single soldiers. The palette and the placer's default object skip entries that are not placeable, so the 59 soldiers no longer appear under SGVOGT_UNIT. Their squads stay under SGVOGT_SQUAD.
- A lone soldier that a map already holds is kept exactly as it was read (the preservation invariant). The game-side guard below is what makes such a map playable.

**Fix, game side (a guard for malformed maps such as mytest.bzm):** `CAILogic::AddObject` gives a soldier loaded without a formation the single-unit formation the game gives any soldier left on its own in play. That is `CreateSingleUnitFormation`, the same call `CFormationDisbandState`, `CCatchFormationState` and `CTransportResupplyHumanResourcesState` make. The formation keeps the soldier's selectability. The guard is an `if`, not an assert, and does not apply in the editor's own engine (`IsEditor`). Scenario units and reinforcements are loaded through the same `AddObject`, so the guard covers them too.

**Commits:**
- `81a6cdaab` fix(03-15): a soldier a map stores on its own gets a formation, not a crash
- `ac5c30e82` fix(03-15): a single soldier goes on a map only inside a squad

**Evidence** (debug stage; the release stage was not touched because Johannes was running it):
- **Reproduced before fixing:** the unfixed debug `Game` on a copy of mytest.bzm stopped with the UBSan error `member call on null pointer of type 'CFormation'` at `SoldierStates.cpp:353:31`, the same stack as the report, exit 134 (`zig-out/local-test/lone-soldier/repro-before.log`).
- **New `map-editor-game-reads-it` checks:** the run now also tries to place `Us_Sniper` and `Allies_Bren` on their own, and places the squads `US_sniper` and `GB_bren_43` next to the Flak38. The game must count the unit plus all 10 squad soldiers.
  - With both fixes switched off, the editor placed both soldiers and the game died: `the game exited code=null signal=6`, with the same UBSan error (`repro-game-reads-it.game.log`).
  - With both fixes, both soldiers are refused with the message `"Us_Sniper" is a single soldier, and the game plays soldiers only in squads: place the squad "US_sniper" (SGVOGT_SQUAD) instead` (and `"GB_bren_43"` for the Bren). The run passed: `PASS (14 units of player 0 near the placed unit and the squads' 10 soldiers; single soldiers refused; game exit 0)`, where 14 = Flak38 with crew 4 + sniper 1 + Bren squad 9.
- **Game guard on Johannes's own map:** the fixed `Game` on the mytest.bzm copy starts the mission. It counted 36 units (player 0: 22, player 1: 14) and exited 0 (`repro-after.log`).
- **`test-map-editor-engine` PASS (260 objects):**
  - The catalogue marks `Us_Sniper` and `Allies_Bren` as not placeable, and `US_sniper` and `10.5-cm_Flak38` as placeable.
  - Both adds are refused, and the message names the squad.
  - The document and the bridge still agree, and the engine still matches the map.
  - The `US_sniper` squad places.
  - The 1 failure noted in the season section above came from the moment in this work when the fix was switched off to reproduce the crash; after that, the test passes.
- **Other tiers:** `test-editor-bridge` PASS, including the season test and the every-game-type add sweep. `map-editor-smoke` PASS (35 steps). `map-editor-host-check` PASS: host check, unknown-object warning and panel smoke for the relative path, the absolute path and `-mod=EditorTestMod`.

**Deviations:**
- The tiers ran on the debug stage in its default copy-data mode, not with `-Dcopy-data=false`. The other session in this worktree builds the same stage in copy mode. Each `-Dcopy-data=false` build deletes the staged Data and links it, and each copy build deletes the link and copies 2.7 GB back. My first `-Dcopy-data=false` run had its Data re-copied while it was running, and `MapEditor` aborted at start in `CClientAckManager::InitConsts` because `Sounds\Ack\acks.xml` was missing. Staying in one mode avoids that.
- Test runs waited for the worktree to be idle (`zig-out/local-test/lone-soldier/run-when-idle.sh`), so they never overlapped the other session's engines.

**For Johannes:**
- Hand-try step 8's "single soldiers show their squad's icon" no longer applies, because single soldiers are no longer listed. 03-09 Task 4's squad-icon fallback stays in the bridge. If you would rather see single soldiers listed and refused on placement, the palette filter in `panels.zig` `loadCatalogue` is a one-line change.
- `AISingleUnitFormation` is still listed under SGVOGT_SQUAD. The MFC editor hid it. Placing it gives a one-man `USSR_Mosin` squad, which is a valid map and not a crash.
- mytest.bzm still holds its `Us_Sniper` unit record. Once the release stage is rebuilt, F5 plays it because of the game guard. To make it a map the original game could also load, delete the sniper and place the `US_sniper` squad instead.

**Still to do:** rebuild the release stage once Johannes has left it, then retry F5 on mytest.bzm.

## Gap fix: missing season textures (hand try, M1)

**Found:** on winter maps (`nSeason = 1`), a mesh unit whose folder has no winter texture was drawn pure white, in the editor and in the game. An example is `Data/Units/Technics/Allies/Artillery/105mm_M2A1_USA`, which has only `1_c/_h/_l.dds`. Of the 242 unit mesh folders, 83 have no `1w` and 166 have no `1a` (counted on `Data/Units`). The original's `textures.pak` lacks them too, so data cannot fix this.

**Root cause:** the unit asks for its season's texture by name:
- `"<path>\1w"`: `MOUnitMechanical.cpp:79`
- the damage states `"<path>\<n>w"`: `:120`
- the passengers' `"1p"` plus the season: `:246`
- the destroyed model's `"2"` plus the season: `:839`
- `MOObject.cpp:57/62` and `MOEntrenchment.cpp:23`

The original DX8 GFX drew a checker texture for a missing file (`GFX/Texture.cpp:97-126`). GFXGPU's `TextureManagerGpu::GetTexture` answers null, and a mesh with no texture is drawn white.

**Fix** (`Sources/src/Scene/VisObjBuilder.cpp`): `CVisObjBuilder::ChangeObject` for `SGVOT_MESH` now gets both of its textures through `GetSeasonedMeshTexture`. That covers `Init` (model and texture) and `SetTexture` (texture only). When the name is missing and its last segment is digits, then an optional `p` or `b`, then a season letter (`^[0-9]+[pb]?[wa]$`), it retries without the letter: `1w`->`1`, `2w`->`2`, `1pw`->`1p`, `1bw`->`1b`, `1a`->`1`. This follows the style of MOBuilding's sprite fallback (`UpdateVisObj`).

Every mesh texture path goes through this function:
- `BuildObject(SGVOT_MESH)` goes through `CreateMeshVisObj` into `ChangeObject`.
- The damage-state, passenger and destroyed-model call sites all call `ChangeObject` or `BuildObject` with a mesh vis type.
- `CVisObjBuilder` is the only `IVisObjBuilder`, so the game and the editor both get the fix.

A texture missing in every season still comes back null, as before.

**Not done (optional miss cache in GFXGPU):** `GetTexture` still probes storage on every miss. A miss cache would outlive a file created later under the same name. The generated-data mount (`NGeneratedData`: template-mission pictures and the briefing picture) creates such files at runtime, and only a mod switch clears the texture manager. After this fix a miss costs one extra probe, and only when an object is built or changes state, never per frame. So the cache was left out as not clearly contained.

**Commit:** `074d4e272` fix(03-15): a unit with no texture for the map's season is drawn in its summer one, not white.

**Evidence** (debug stage in its default copy-data mode; the release stage was not touched because Johannes was running MapEditor from it):
- New engine-tier test `TestMissingSeasonTextureFallsBack` in `tools/zig/editor_bridge_test.cpp`, run right after `TestSeasonPicksTheVisuals`. On coldwinter it places `105mm_M2A1_USA` and `10.5-cm_Flak38` on bare ground, at least 160 px apart. For each gun it reads the mesh texture it is drawn with, through `IScene::Pick` on a box round the gun and a visitor that counts meshes only. It then takes the M2A1's mean drawn colour from the pixels its placing changed.
- **Before the fix** (test written first; `zig-out/local-test/season-fallback/before-fix-editor-bridge.out`): the M2A1's mesh was drawn with no texture (`<none>`), at a mean RGB of 211,222,216 over 1383 pixels, which is white. The Flak38 was drawn with `10_5_cm_flak38\1w`. That run's check also counted the icon sprites' `<none>`, which failed the Flak38 as well, so the check was narrowed to meshes before the fix went in.
- **After the fix** (`fixed-editor-bridge.out`): the M2A1 is drawn with `units\technics\allies\artillery\105mm_m2a1_usa\1`, at a mean RGB of 107,114,76 (olive). The Flak38 is still drawn with `\1w`. `test-editor-bridge` PASS, including the season test (coldwinter: Flak38 1/1 and infantry 10/10 winter; arnheim: summer).
- **Captures** (640x480):
  - `zig-out/local-test/season-fallback/m2a1-winter-BEFORE-fix.png` (white gun at the left, grey Flak38 at the right)
  - `m2a1-winter-AFTER-fix.png`
  - side by side: `m2a1-winter-before-after-crop.png`
  - also `.tga`, plus `m2a1-winter-empty.tga` for the empty ground
- **Other tiers** (`season-fallback/verify-all.out`):
  - `test-map-editor-engine` PASS (260 objects)
  - `map-editor-smoke` PASS (35 steps)
  - `map-editor-host-check` PASS: host check, unknown-object warning, panel smoke
  - `map-editor-game-reads-it` PASS (14 units of player 0, single soldiers refused, game exit 0)

**Still to do:** rebuild the release stage once Johannes has left it, then look at a US gun on coldwinter in the release MapEditor and Game.

## Gap fix: switching mod closes the map (hand try, step 7)

**Found:** a map opened and saved under AchtungPanzer2, then File > Mod > None, showed 1359 unknown objects. File > Mod (plan 03-08, D-26) reopened the current map under the new mod, so a map read under one object database was shown under another.

**Johannes's decision** (D-26 revised 2026-09-29, recorded in `03-CONTEXT.md` and the spec): switching closes the map.
1. A dirty map asks first (D-23): Save / Don't save / Cancel. Cancel means no switch. Save on a new, shipped or read-only map goes through Save As, and cancelling that Save As cancels the switch.
2. Then the map is closed and the mod switched. The editor has no document: the title is "Map Editor [<mod>]", the status bar says "no map open", the undo history is cleared, autosave is idle, and the view and per-map panels are as at a start with no map. The palette reloads from the new mod.
3. The editor stays empty. The Open dialog starts in the new mod's user maps folder (`<user root>mods/<Folder>/maps`, or `<user root>maps` for None). This already worked through `state.modFolder()`, and the smoke now asserts it.
4. Choosing the mod that is already active does nothing: no prompt, no close.

**Change** (commit `68cd1d79e`):
- `core/editor.zig`: `Editor.close()` forgets the document, history, selection and status.
- `panels_logic.zig`: `requestSwitchMod(folder, active)` ignores the active mod (`isSameMod`). `switchModClosingMap` calls the bridge's `setMod` and then closes the document on `ok`, and also on `failed`, because the engine's map is already gone then. On a refusal (`refused`, `bad_argument`, `no_session`) it keeps the map and the mod, as `BkEditorSetMod` checks before it touches anything.
- `panels.zig`: `performModSwitch` closes through `switchModClosingMap`, then `mapClosed`: `view.closeMap()` (remembers the camera per D-15, clears map size, gestures and the placer's old-database object), `mapOpened()` resets the per-map panels, autosave's dirty clock is cleared, and the closed map's recovery copy is deleted. That is the same rule as a clean or Don't-save quit (D-22); a Save through the prompt had already deleted it. The palette is reloaded (`catalogue_generation` counts reads).
- A running test game is its own process on its own copy (D-01/D-03) and keeps running. `pollTestGame` still reports its exit, and Test in game with no map open does nothing, as before.
- The bridge needed no change: `BkEditorSetMod` already closes the open map and works with none open. `test-editor-bridge` now also switches to None and back with no map open.

**Evidence** (debug stage in its default copy-data mode; the release stage Johannes is running was not built into or run from):
- `test-editor-core` 86/86 passed, including the new `Editor.close` test.
- `test-map-editor-panels` 52/52 passed. The new tests cover: Cancel keeps the map and mod; Don't save closes and switches, then a switch with no map open goes straight through with None reaching the bridge as null; Save waits for the save and then switches; a failed Save or a cancelled Save As cancels the switch and a landed Save As lets it through; the same mod on a dirty map is a no-op; a refusal keeps the map and a failure partway closes it; `isSameMod`.
- `test-map-editor-auto` 8/8 passed.
- `map-editor-smoke` PASS, 44 steps (9 new, on the tracked fixture `EditorTestMod`, which the build now stages for the smoke). They run on the dirty read-only foreign map with an active recovery copy:
  - None (already active) changes nothing.
  - File > Mod EditorTestMod asks; Cancel keeps the map and mod.
  - Asked again, Save becomes Save As; cancelling it cancels the switch.
  - Asked a third time, Don't save closes the map and switches. Checked afterwards: no document, no history, no selection, no view map, no tiles, no sounds, no unknown objects; the panels and the bridge both on EditorTestMod; palette re-read and equal to the bridge's catalogue; Open folder `.../mods/EditorTestMod/maps`; autosave idle; recovery copy deleted; the foreign file untouched.
  - None with no map open switches back without asking; Open folder `.../map-editor-smoke-user/maps`.
- Negative control, reverted afterwards: with `switchModClosingMap` not closing on `ok`, the smoke fails at "Don't save closes the map and switches the mod: ...coldwinter.bzm is still open".
- `map-editor-host-check` PASS on all three runs. The `-mod=EditorTestMod` run adds a File > Mod leg through `panels.act`: the active mod is a no-op; None on a map dirtied by a sound asks and changes nothing; Don't save closes the map and switches; switching back with no map open works. It prints `map-editor: mod switch PASS (EditorTestMod -> None -> EditorTestMod; asked first, map closed, palette 5559 -> 5559 entries)`.
- `test-editor-bridge`: every check passed except one, and nothing in the mod test failed (including the new switches with no map open). The failing check is `TestMissingSeasonTextureFallsBack`: "the placed 105mm_M2A1_USA ... is drawn with ...\1 (drawn with: ...\1w)". It is not caused by this change. The worktree's untracked, generated `Data/Units/**/1w_*.dds` (from `season-textures`, commit `c5cff9022`) now give the M2A1 a winter texture, so the fixture that expected a missing one no longer holds. Logged in `deferred-items.md`.
- Logs: `zig-out/local-test/modclose-*.log`.

**Still to do:** rebuild the release stage once Johannes has left it, then retry hand-try step 7: open a map under AchtungPanzer2, then File > Mod > None. The prompt should ask if the map is dirty, then the editor should be empty.

## Gap fix: tile pictures in the Brush's tile picker (hand try, M1)

**Found:** Tools > Brush > "tile" was a combo of "tile 0", "tile 1", and so on. Johannes: "It would be great to see the tile as graphics next to the name and number; otherwise hard to choose and trying out several 100 is not feasible." Coldwinter's tileset offers 184 tiles.

**How the engine and the MFC editor get a tile's picture:** a paint cell's tile is an index into the tileset description's `<tilemaps>`. Each entry has four texture-space corners of an isometric diamond: `maps0` top, `maps1` right, `maps2` left, `maps3` bottom. The texture is `<szTilesetDesc>_h.dds`, for example `terrain\sets\2\tileset_h.dds`, 256x512 with 64x32 tiles. The MFC tile palette (`MapEditor/TabTileEditDialog.cpp`, `CreateImageList`) cut each thumbnail as the bounding box of those corners, flipped it when `maps3` lies above `maps0`, and masked it with `editor\terrain\tilemask.tga`. The terrain type (`<terrtypes>`: "Icicle", "Ice", "Snow", "Asphalt", ...) is the only name a tile has.

**Change:**
- **Bridge** (commit `54cc41060`, `Sources/src/EditorBridge/bridge.h/.cpp`; C ABI through `Guarded`, documented in `bridge.h`):
  - `BkEditorDescribeTile(session, tile, BkEditorTile*)` gives the tile's terrain type (name and index) and the tileset's storage name.
  - `BkEditorTilePicture(session, tile, rgba, capacity, max_side, &w, &h)` returns the tile's diamond as RGBA8, top row first (`BkEditorObjectPicture`'s layout), transparent outside the diamond with a one-pixel fade. It reads the corners from the tileset's own `.xml`, because the engine's loaded copy is pulled in by `CorrectUVMaps` by an amount that depends on the screen width. It reads the texture from `_h.dds`, else `_c`, else `_l`. The decoded texture and description are cached per tileset for the session, and `BkEditorSetMod` drops them.
  - Refusals: tile outside 0..255 or `max_side` outside 8..256 is `BAD_ARGUMENT`. No map open, a tile no terrain type lists, or a short buffer (real size still reported) is `REFUSED`.
  - `BkEditorObjectPicture`'s scale-and-write tail is shared as `WritePicture`.
- **App** (commit `043611098`):
  - The current tile's picture sits beside the combo, and the combo reads "tile 0 - Icicle".
  - Opening the combo shows an opaque grid of every tile's picture with its number, in sections per terrain type ("Icicle (7)", "Ice (12)", ...) in the tileset's order. The current tile is outlined and scrolled into view, each cell has a tooltip naming it, and a click chooses the tile.
  - Pictures come through `pictures.zig` as a second cache (`Source.tile`, keyed by the tile number). They are requested only for cells in view, pumped 32 per frame, and dropped when a map with another tileset opens or the mod switches.
  - Pure logic in `panels_logic.zig`: `sortTilesForPicker`, `nextTileGroup`, `indexOfTile`, `gridColumns`, `tileLabel`, `tileKey`/`tileFromKey`, `tilePicturesStale`.

**Evidence** (debug stage in its default copy-data mode; the release stage was not built into or run from):
- Engine tier, new `TestTilePicturesAndClose` on coldwinter (`tilepick-editor-bridge-2.log`):
  - 184 tiles, 184 distinct pictures, 16 terrain types (Icicle, Ice, Blizzard, Dirty Snow, Snow, Ground, Snowstorm, Snowdrift, Slush, Blanket, Garden, Snowfield, Cube, Iceberg, Bottom, Asphalt).
  - Every picture passed its checks: opaque middle, transparent corners, not one flat colour, not black.
  - The first tiles of different terrain types all differ.
  - The refusals follow the contract, and `max_side` 16 scales a picture down while keeping it wider than tall.
  - Timing: 5.7 ms for the first picture (decodes the texture), then 0.14 ms each.
  - All 184 tiles side by side: `zig-out/local-test/03-15-tile-pictures.png` (and `.tga`).
- `test-map-editor-panels` 64/64 passed (in the shared working tree). The 4 new picker tests cover order and sections, labels, grid columns, and keys plus the stale-tileset rule.
- `map-editor-smoke` PASS, 52 steps. After the mod steps it reopens the saved map, clicks the tile combo, and waits until every cell in view has its picture ("the tile picker shows 39 of 184 tiles, each with its picture"). It captures the frame, clicks a tile of another terrain type, and checks the brush took it and the popup closed.
- **Capture of the open picker for Johannes:** `zig-out/local-test/03-15-tile-picker.png` (1280x800; `.tga` beside it).

## Gap fix: File > Close (hand try, M1)

**Found:** File had no Close. The only ways to get an empty editor were File > Mod or quitting.

**Change** (bridge part in `54cc41060`, app part in `043611098`):
- **Menu:** File > Close sits between Save As... and Autosave. The shortcut is shown as Cmd+W on macOS and Ctrl+W elsewhere; either modifier works on every platform, like Cmd/Ctrl+Z. It is not taken while a text field is being typed in, as with F5. The item is disabled, and the shortcut does nothing, with no map open.
- **Prompt:** Close is guarded by the unsaved-changes prompt like Open (D-23; `Pending.close` / `Step.close`).
  - Cancel keeps the map.
  - Save on a shipped, read-only or new map goes through Save As. A failed Save or a cancelled Save As keeps the map; a landed one closes it.
  - Don't save closes the map.
- **Bridge:** `BkEditorCloseMap` is `CloseSessionMap` without the mod swap. The world's objects and the terrain leave the scene, and the per-map tables are reset. It is OK with no map open.
- **App:** `closeMapAndDocument` calls it, then `Editor.close()` (from `68cd1d79e`), then `mapClosed` with the same bookkeeping as File > Mod:
  - the view is as with no map, and the per-map panels are reset
  - the undo history is cleared and autosave is idle
  - the recovery copy is deleted on Don't save
  - the title reads "Map Editor [<mod>]" and the status bar "no map open"

  The document closes even if the bridge refuses or fails, so it never claims a map the engine may not have. The mod and the palette stay.

**Evidence:**
- `test-map-editor-panels`, 4 new tests: a clean close; Cancel keeps the map and Don't save closes it; the Save paths (a failed Save and a cancelled Save As keep the map, a landed Save or Save As closes it); a bridge refusal or failure still closes the document.
- The engine tier checks that after `BkEditorCloseMap` the tileset, tile pictures and objects are refused as "no map is open", that a second close is harmless, that a frame still draws, and that coldwinter reopens with the world matching. `BkEditorCloseMap` with no map open is OK. The NO_SESSION list now has 52 entry points and the "no map is open" list 31.
- `map-editor-smoke` steps on the dirty reopened map:
  1. File > Close asks.
  2. Cancel keeps the map, still dirty.
  3. Ctrl+W, as a real key event through ImGui, asks too.
  4. Don't save closes the map. Afterwards: no document, history, selection, view map, tiles or sounds; the bridge's tileset refused as "no map is open"; title "Map Editor"; autosave idle; no recovery copy active; mod unchanged.

**Other tiers** (logs `zig-out/local-test/tilepick-*.log`):
- `test-editor-core` 86/86.
- `test-map-editor-engine` PASS (260 objects).
- `map-editor-host-check` PASS on all three runs, plus the mod switch leg.
- `test-editor-bridge`: every check passed except the known `TestMissingSeasonTextureFallsBack` (the generated `1w` textures in this worktree; see the section above).

**Note for review:** another session was changing map-sound code in the same working tree at the same time: the Sounds panel note, `defaultSoundName`, a smoke check, `main.zig`, `testlaunch.zig` and the engine sound files. Those hunks were left unstaged. The two commits above contain only this fix. The tiers ran on the shared tree with both sets of changes.

**Still to do:** rebuild the release stage once Johannes has left it, then try the picker and File > Close by hand. The picker's grid is six 64-pixel columns and fills about 60% of the window's height.

## Open items for Johannes to decide (plan 5 carried, not closed by any plan of phase 3)

Still open in `.planning/WINDOWS.md` (entries 1-3, ledger `open_count: 3`):

1. The status line is never cleared after a later success (`Sources/editor/app/panels.zig`).
2. No test of `view.zig`'s event-to-tool wiring beyond the routing function (`Sources/editor/app/view.zig`).
3. The scroll-direction unit test restates its own constants; an engine-tier ScreenToWorld direction check would catch a sign error (`Sources/editor/app/view_math.zig`).

They block `/gsd-ship` while `windows_enforce` is on. Options: a small follow-up plan, or waive each with a reason. Not decided here.

Other known limits carried in the phase summaries: `map-editor-auto` is local-only (not in CI); `bk_imgui_backend_use_global_mouse` cannot fully disable ImGui's private cursor fallback (03-12); the renderer's own capture-failure string is not reachable through IGFX (03-13); the Windows Test-in-game and cursor-isolation paths were not exercised on a real Windows desktop.

## Decisions Made

See key-decisions in the frontmatter.

## Deviations from Plan

None - plan executed as written. The plan's own CI-run instruction was followed with an explicit `gh workflow run`, since a push alone does not trigger the workflow on this branch.

## Issues Encountered

- `.planning/REQUIREMENTS.md` is the old port-level document and has no `M1-EXIT-*` IDs, so `requirements mark-complete` is a no-op for them, as in plans 05, 11 and 14. `M1-EXIT-HANDTRY` stays unmarked until Johannes reports.
- The worktree's untracked `.gsd/` and `.planning/milestone.lock` were left alone.

## Known Stubs

None.

## User Setup Required

None.

## Next Phase Readiness

- M1's automated exit criteria are met. Remaining: Johannes's hand try, then `/gsd-verify-work` for phase 3 (harvesting the `human_judgment` items across the summaries), then the decision on the three open ledger entries.
- On "approved", a continuation records the result here and marks the spec's hand-try line met; on failures, the failing numbers become gaps for `/gsd-verify-work`.

---
*Phase: 03-map-editor-plan-6-finish-m1*
*Completed: 2026-09-29*

## Self-Check: PASSED

- Spec exists and holds the commit: `git log` shows `a8ee30d10` on `feat/map-editor-plan-6`, pushed (origin at the same head when CI ran).
- CI run 36473046568 head SHA equals `a8ee30d10...`; six jobs success.
- Release stage: `Game` and `MapEditor` present; `03-15-t3-check.log` holds one `host check PASS`.
- `plan_head_before` 87a2028f0; `git rev-list --count 87a2028f0..a8ee30d10` = 1, matching `actuals.commits`.

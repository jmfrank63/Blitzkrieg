---
phase: 05-map-editor-m3-random-map-templates-minimap-tools-parity
plan: 10
subsystem: map-editor
tags: [map-editor, m3, rmg-composers, field-sets, templates, check, round-trip, determinism, authored-end-to-end, parity, zig, cpp]
status: complete

requires:
  - phase: 05-map-editor-m3-random-map-templates-minimap-tools-parity
    plan: 03
    provides: ListRmgFolder, the O4 object filters the objects tab uses
  - phase: 05-map-editor-m3-random-map-templates-minimap-tools-parity
    plan: 08
    provides: BkEditorCreateRandomMap, the determinism harness, the engine-hosted tool builder
  - phase: 05-map-editor-m3-random-map-templates-minimap-tools-parity
    plan: 09
    provides: the user RMG root and the "RMG_USER" mount, rmg.Document, the container and graph records, the composer commands and panels pattern
provides:
  - BkEditorRmgReadFieldSet / WriteFieldSet (SRMFieldSet) and BkEditorRmgReadTemplate / WriteTemplate (SRMTemplate with the QuickLoadMapInfo written beside it), typed records with counted two-pass arrays, plus BkEditorRmgTileset and BkEditorRmgFileExists
  - core rmg.zig FieldSet and Template with their Check! rules and explicit fixes (Fix all is one undo step), composers.zig field-set and template composer state, Editor.readFieldSet/writeFieldSet/readTemplate/writeTemplate/tilesetTypes
  - Tools > Fields Composer (terrain, objects and heights tabs) and Tools > Templates Composer (lists with weights, default field, VSO width and opacity, Diplomacy, Units grid, script, MOD, Check!), the rmgf_* and rmgt_* auto commands and predicates
  - the full four-kind round trip (43 templates, 102 graphs, 404 containers, 27 field sets), the authored end-to-end (byte-identical generation, the game loads the map), CI wiring for both data-only steps
  - PARITY rows R7-R13 closed
affects: [05-11]

commits: 3
plan_head_before: fcf28bda02f8912bfc94e107f9452d81ddd9aed1
actuals:
  tokens: 125000
  tasks: 3
  commits: 3
  files: 24

tech-stack:
  added: []
  patterns:
    - "A template Check! reads the graphs and field sets it names through the storages and lists their findings with their own severity under the nested file's name; the template's own rules are the findings without a prefix"
    - "Every composer write is read back through the storage and compared before OK; the QuickLoadMapInfo of a template is written in the same step and compared with what FillFromRMTemplate makes of the template"
    - "A header-only shared reader (tools/zig/rmg_record_io.h) holds the two-pass counted-array reads, so the round trip and the determinism tool author and compare with one copy of the ABI handling"
    - "An authored end-to-end is a copy of a shipped set under new names written through the composers' own I/O, rewritten after each wipe of the scratch user folder"

key-files:
  created:
    - tools/zig/rmg_record_io.h
  modified:
    - Sources/src/RandomMapGen/WV_Types.h
    - Sources/src/EditorBridge/bridge.h, bridge.cpp, session.h, session_rmg.cpp
    - Sources/editor/core/rmg.zig, composers.zig, bridge.zig, editor.zig, fake_bridge.zig
    - Sources/editor/app/c_bridge.zig, c_bridge_test.zig, commands.zig, panels.zig, panels_m3.zig, game_reads_m3.zig
    - tools/zig/editor_bridge_test.cpp, composer_roundtrip_test.cpp, rmg_determinism_test.cpp
    - build.zig
    - .github/workflows/cross-platform.yml
    - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-PARITY.md, deferred-items.md

key-decisions:
  - "Check! for templates is implemented (D-12) and recorded as an addition: the MFC's handler at RMG_CreateTemplateDialog.cpp:1269 does nothing. Findings carry explicit fixes (remove a graph, field set or VSO that cannot load, zero a negative weight, clear a default field outside the list, clamp a VSO width or opacity, resize the unit list to the players, take the header from the first graph); nothing is rewritten silently"
  - "A template names its graphs and field sets by storage name and the composer never creates them; a name the storages do not hold is a finding whose fix removes it"
  - "The season folder mapping was wrong since 05-09 (CMapInfo::SEASON_FOLDERS is 1 summer, 2 winter, 3 africa, 4 spring, with REAL_SEASONS {0,1,2,0}); fixed once in rmg.zig and used by both composers"
  - "CWeightVector carries IsConsistent(): a write is refused when a vector's elements and weights differ in length, so a malformed shipped file cannot be re-saved into a worse one"
  - "Save All is Save (one file per composer, as for the containers and graphs)"
  - "The authored game leg uses the real engine through the core Editor: author, createRandomMap with a fixed seed, open, test copy, play under BK_AUTO_UI; it sets a scratch XDG_DATA_HOME where the platform honours it"

requirements-completed: [D-06, D-07, D-12, D-37, D-40]

duration: not recorded (the session was compacted once, so the start time was lost); the engine tier and the gates took the largest share
completed: 2026-10-03

coverage:
  - deliverable: "D-06/D-07 Fields Composer reading and writing SRMFieldSet; the terrain, objects and heights tabs"
    verification:
      - kind: test
        ref: "editor_bridge_test.cpp#TestM3RmgFieldSets (editor-bridge: M3 rmg field sets ok): two-pass read equals the engine's own load, a write lands under the user RMG root and reads back, a shipped name is refused, bounds and an inconsistent weight vector are refused"
        status: pass
      - kind: test
        ref: "rmg.zig and composers.zig core tests (shell and tile edits, the Check! rules and their fixes, Fix all as one undo step); c_bridge_test: all 27 shipped field sets read through the ABI and Check! reports 0 errors"
        status: pass
      - kind: command
        ref: "map-editor-m3-auto frames 1214-1308 (rmgf_window, rmgf_new/open, tab switches, shell and tile edits, rmgf_check -> rmgf_errors:1 -> rmgf_fix_all -> rmgf_errors:0, rmgf_saveas); rc 0, no FAIL"
        status: pass
    human_judgment: false
  - deliverable: "D-06/D-07/D-12 Templates Composer; Check! implemented as a recorded addition (R13); a save writes the Template and the QuickLoadMapInfo"
    verification:
      - kind: test
        ref: "editor_bridge_test.cpp#TestM3RmgTemplates (editor-bridge: M3 rmg templates ok) and c_bridge_test (43 shipped templates read; the template's own Check! rules clean on all of them; a crafted template with ghost names, empty weights and a bad VSO gives each finding with its fix)"
        status: pass
      - kind: command
        ref: "map-editor-m3-auto frames 1310-1400 (rmgt_window/new/open, weight, default field, VSO, Diplomacy and Units popups with shots, rmgt_check, rmgt_saveas); rc 0, no FAIL"
        status: pass
    human_judgment: false
  - deliverable: "D-40.4 all four kinds round-trip equal and byte-stable on the data-only tier"
    verification:
      - kind: command
        ref: "zig build test-rmg-composer-roundtrip -Dtarget=aarch64-macos: 'composer-roundtrip: 43 templates, 102 graphs, 404 containers, 27 field sets ok' / PASS (the bridge's record equals the engine's own load, the copy reads back equal, a second write holds the same bytes, and each template's QuickLoadMapInfo equals FillFromRMTemplate's)"
        status: pass
    human_judgment: false
  - deliverable: "D-40.5 an authored set generates byte-identical maps from a fixed seed and the game loads the result"
    verification:
      - kind: command
        ref: "zig build test-rmg-determinism: 'rmg-determinism: authored set byte identical ok (seed 424242, graph 0, angle 0)' and PASS; the earlier legs (same seed, open graph, blank seed) unchanged"
        status: pass
      - kind: command
        ref: "zig build map-editor-game-reads-it-m3: 'the authored set generated a map the game loaded clean: 16 roads, exit 0' and 'game reads it M3 PASS' (the short-railroad and regular-map legs still pass)"
        status: pass
    human_judgment: false
  - deliverable: "D-40.8 guard and the hermeticity of build.zig"
    verification:
      - kind: command
        ref: "zig build test rc 0 with no FAIL; zig test tools/zig/build_hermeticity_test.zig 3/3; test-editor-bridge, map-editor-m3-auto (579 actions) rc 0"
        status: pass
    human_judgment: false
  - deliverable: "PARITY rows R7-R13 closed with evidence"
    verification:
      - kind: command
        ref: "05-PARITY.md Evidence cells filled for R7-R13; every command and predicate named in them exists in the m3-auto frame list"
        status: pass
    human_judgment: false
  - deliverable: "Windows leg (D-37)"
    verification:
      - kind: command
        ref: "win-home (x86_64-windows-msvc, branch head 00c02ae11, no GUI): test-editor-core 373/373, the app tiers 211/213 (the 2 usual skips), the MSVC compile of test-editor-bridge, test-rmg-determinism and test-rmg-composer-roundtrip, and the editor app build, all rc 0"
        status: pass
      - kind: command
        ref: "the composer frames of map-editor-m3-auto on Windows need a GUI session: not run over ssh; left to CI or a hand run (WINDOWS.md entry 6)"
        status: pending
    human_judgment: true
    rationale: "win-home runs no GUI over SSH (executor rules)"
---

# Phase 05 Plan 10: Fields and Templates composers, the four-kind round trip and the authored end-to-end - Summary

One-liner: the Fields Composer and the Templates Composer - the MFC's tabs and columns over the engine's own SRMFieldSet and
SRMTemplate (the template written with its QuickLoadMapInfo), and a Check! for templates the MFC never had, with explicit one-undo-step
fixes - and the proof that closes the composers: all 27 field sets, 43 templates, 102 graphs and 404 containers round-trip equal and
byte-stable, and an authored template, graph, container and field set generate the same map twice from one seed, which the real Game
loads.

## Accomplishments

- **Task 1 (`f37d947da`):** the field set record entries and their two-pass arrays, the tileset and file-exists reads, `FieldSet` and its
  Check! rules in `rmg.zig`, the field-set composer state, the Fields Composer window (terrain, objects and heights tabs, shell lists,
  the tile and object pickers with the O4 filter, Check!, the File row), `rmgf_*` commands, `TestM3RmgFieldSets`, the season-folder fix,
  the weights-consistency guard.
- **Task 2 (`25feb3962`):** the template record entries (with the unit-creation mapping and the QuickLoadMapInfo), `Template`, its
  Check! with eight explicit fix kinds, the Templates Composer window (header, graph, field and VSO lists with weights, the default
  field, the Diplomacy popup, the Units grid with appear points, script and MOD), `rmgt_*` commands and the `rmgt_popup` shots,
  `TestM3RmgTemplates`, the 43 shipped templates through the ABI.
- **Task 3 (`00c02ae11`):** `composer_roundtrip_test.cpp` with all four kinds (the readers moved to `tools/zig/rmg_record_io.h`), the
  authored leg of `rmg_determinism_test.cpp`, the authored game leg of `map-editor-game-reads-it-m3`, the two data-only steps in the
  Windows and macOS engine jobs of `cross-platform.yml`, PARITY R7-R13, the deferred items.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] The season folder mapping from 05-09 was wrong**
- **Found during:** Task 1, reading `CMapInfo::SEASON_FOLDERS` for the field set's season
- **Issue:** `rmg.zig` named the folders from a table that did not match the engine's (1 summer, 2 winter, 3 africa, 4 spring; `REAL_SEASONS` = {0,1,2,0}); a winter or spring file was labelled with the wrong folder.
- **Fix:** `season_folders`, `real_seasons`, `seasonName` (the fourth folder is `\4\`) and `seasonIndex` follow the engine; the composers and the Check! rules use them.
- **Files modified:** Sources/editor/core/rmg.zig
- **Commit:** f37d947da

**2. [Rule 2 - Missing critical functionality] A weight vector whose elements and weights differ in length could be re-saved**
- **Found during:** Task 1, writing a field set through `CWeightVector`
- **Issue:** the engine's vector keeps cumulative weights in a second array; a malformed file loads with the two out of step and a save would write the damage back.
- **Fix:** `CWeightVector::IsConsistent()` (WV_Types.h) and the bridge writes refuse an inconsistent vector with a message; a test covers it.
- **Files modified:** Sources/src/RandomMapGen/WV_Types.h, Sources/src/EditorBridge/session_rmg.cpp
- **Commit:** f37d947da

**3. [Rule 1 - Bug] An opened template differed from its fixture after the unit-creation mapping**
- **Found during:** Task 2's core tests
- **Issue:** `UnitCreation.slot_count` was carried from the engine record and made a read template unequal to the one written.
- **Fix:** `unitFromRmg` and the fake bridge's `unitFromRecord` set `slot_count = 0` (the template record has no slots).
- **Files modified:** Sources/editor/core/editor.zig, fake_bridge.zig
- **Commit:** 25feb3962

**4. [Rule 1 - Bug] `Template.setDiplomacy` aliased its input**
- **Found during:** Task 2's core tests (`@memcpy arguments alias`)
- **Issue:** the call is made with the template's own table.
- **Fix:** the input is duplicated first.
- **Files modified:** Sources/editor/core/rmg.zig
- **Commit:** 25feb3962

**5. [Rule 1 - Bug] Check! rules that fired on shipped data**
- **Found during:** Tasks 1 and 2 against the real engine's shipped files
- **Issue:** "objects shell holds no objects" fired 30 times on deliberate gaps in shipped field sets; a template whose script lists differ from its first graph's gave an error on shipped templates.
- **Fix:** the first rule is removed; the second is a warning with a take-from-graph fix. The engine tier asserts the template's own rules clean on all 43.
- **Files modified:** Sources/editor/core/rmg.zig
- **Commit:** 25feb3962

### Differences from the plan's text (recorded, not bugs)

- **Check! for templates is an addition.** The MFC handler at RMG_CreateTemplateDialog.cpp:1269 does nothing; D-12 asks for it and PARITY R13 records it as NF (added anyway).
- **Wiring the two data-only steps into the workflow is done here.** 05-09 left it to the orchestrator; Task 3's text names it, so the steps run in the Windows and macOS engine jobs (the only two jobs that run the engine tier). A manual dispatch of the workflow on the branch is recorded under Verification below.
- **The authored game leg lives in game_reads_m3.zig,** not game_reads_common.zig: the plan allowed extending `map-editor-game-reads-it-m3`. It authors through the core Editor on the real bridge and plays the map under BK_AUTO_UI with BK_MAP_TRACE; the build step gives it a scratch `XDG_DATA_HOME`.
- **The authored set is a renamed copy of a shipped one** (template02 summer, its first graph, its default field set, an authored copy of each container the graph's nodes hold), rewritten through the composers' I/O after each wipe of the scratch folder.
- **The record readers moved into `tools/zig/rmg_record_io.h`** so the round trip and the determinism tool share one copy.
- **Task commits.** Tasks 1 and 2 shared several files (`bridge.h`, `rmg.zig`, `panels_m3.zig`, ...). Task 2's additions were set aside as copies, the tree was brought back to the Task 1 state, verified (compile, core 368, the engine tier) and committed, then the full files were restored for Task 2.
- **Windows:** the GUI composer frames did not run on Windows (see above).

**Total deviations:** 5 auto-fixed (4 bugs, 1 missing critical functionality), the rest recorded. **Impact:** the season fix corrects 05-09's labelling; nothing else changes the plan's contract.

## Issues Encountered

- BSD `sed -i` on macOS needs a backup suffix argument and failed on a multi-line script; the edits were made with Python instead.
- Zig 0.16: a capture may not shadow a declaration (`objects`, `catalogue`, `entry`, `listed`, `cell` were renamed); `if (a) if (b) |x| {} else |_| {}` needs braces; `{d:>4}` on a signed integer prints a `+`, so the new UI code uses plain `{d}`.
- A background shell command with an inner `&` returned at once; the long runs used `nohup sh -c '...; echo rc=$?'` and a poll on the log.
- Gate runs were serialised after the 05-09 overlap lesson; none overlapped.

## Deferred Issues

See `deferred-items.md` ("From 05-10"): no thumbnails in the Fields tabs, Save All is Save, appear points listed in tiles, the authored game leg's user root cannot be redirected on Windows, the composer frames have no Windows run, the nested findings a template's Check! shows, and the template composer not creating the graphs and field sets it names.

## Verification

- `zig build test` rc 0, no FAIL; `zig test tools/zig/build_hermeticity_test.zig` 3/3.
- `zig build test-rmg-composer-roundtrip`: `composer-roundtrip: 43 templates, 102 graphs, 404 containers, 27 field sets ok`.
- `zig build test-rmg-determinism`: the three earlier legs and `rmg-determinism: authored set byte identical ok (seed 424242, graph 0, angle 0)`.
- `zig build map-editor-game-reads-it-m3`: PASS, the authored map loads with 16 roads and the game exits 0.
- `zig build map-editor-m3-auto`: 579 actions, rc 0, no FAIL (the composer frames are 1214-1400).
- win-home (no GUI): core 373/373, app tiers 211/213, the MSVC compile of the engine tier and both composer tools, the editor app build.
- CI (workflow dispatched on the branch at 00c02ae11, run 37072698456): macOS Engine tier, Random missions tier, RMG composer round trip and RMG determinism green; the Linux, mingw and macOS-Intel jobs green. The Windows job is red in its Engine tier with the same two failures as the run of 15:43 (37029035801), before this plan: `the same seed regenerates a byte-identical map` and `the seed a blank draw reported regenerates that map` (the road and river point padding the debugging branch `fix/rmg-windows-determinism` zeroes, cabb77da8). The Windows job stops there, so this plan's two new Windows steps did not run; WINDOWS.md entry 7 holds that and the Windows composer frames. Nothing in this plan changed the Windows generation path.

## Known Stubs

None. The thumbnails the MFC's tabs drew are a recorded gap (names and indices are listed instead), not a stub: every list and edit works.

## Threat Flags

None. The new write surface (field sets and templates under the user RMG root) is the one T-05-10-02 covers: writes resolve only under the user root, a shipped name is refused, and every write is read back and compared.

## Self-Check: PASSED

Checked: `tools/zig/rmg_record_io.h`, `tools/zig/composer_roundtrip_test.cpp`, `Sources/editor/app/game_reads_m3.zig` and `05-PARITY.md` exist; commits `f37d947da`, `25feb3962` and `00c02ae11` exist; `git rev-list --count fcf28bda0..HEAD` was 3 when `commits:` was written.

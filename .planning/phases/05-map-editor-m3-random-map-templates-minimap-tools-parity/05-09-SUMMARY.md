---
phase: 05-map-editor-m3-random-map-templates-minimap-tools-parity
plan: 09
subsystem: map-editor
tags: [map-editor, m3, rmg-composers, containers, graphs, canvas, user-rmg-root, storage-mount, round-trip, parity, zig, cpp]
status: complete

requires:
  - phase: 05-map-editor-m3-random-map-templates-minimap-tools-parity
    plan: 03
    provides: ListRmgFolder, the storage scan the composers' Open lists use
  - phase: 05-map-editor-m3-random-map-templates-minimap-tools-parity
    plan: 08
    provides: BkEditorListStorageFiles, the engine-hosted tool builder, the XDG-redirected m3-auto user root
provides:
  - the user RMG storage root (D-09, costly): <UserRoot>rmg/ (or <UserRoot>mods/<Folder>/rmg/ with a mod active) mounted as the "RMG_USER" layer over Data and below the mod layer by BkEditorStart and BkEditorSetMod, remounted when the platform's user root moves; shipped RMG files are read-only (a write to one is REFUSED with the Save-As message) and every write lands under that root only
  - BkEditorRmgReadContainer / WriteContainer / ReadGraph / WriteGraph (typed records with counted arrays, two-pass), BkEditorRmgPatchInfoRead, BkEditorRmgImportPatch (D-10 copy-in, dry run then apply) and BkEditorRmgRoot - the engine's own SRMContainer / SRMGraph serialisers behind them, never re-typed
  - core rmg.zig (Container, Graph, the Check! rules, the canvas state machine, composer documents with their own undo), composers.zig (the two composers' working state, std-only over the Editor) and Editor.readContainer/writeContainer/readGraph/writeGraph/importPatch/rmgSource
  - Tools > Containers Composer and Tools > Graphs Composer (the MFC columns, File New/Open/Save/Save As, the patch picker and Browse with the copy-in YES/NO, tri-state patch properties, the draw-list canvas, node and link properties, Check! with explicit one-undo-step fixes) and the named commands rmgc_* / rmgg_* with their predicates
  - tools/zig/composer_roundtrip_test.cpp + the data-only step test-rmg-composer-roundtrip (D-40.4 first half: 404 containers, 102 graphs)
  - PARITY rows R1-R6 and R14 closed
affects: [05-10, 05-11]

commits: 3
plan_head_before: 13803803be0ba653a787da0a3f8aae89b08fc03d
actuals:
  tokens: 105700
  tasks: 3
  commits: 3
  files: 21

tech-stack:
  added: []
  patterns:
    - "A composer record crosses the ABI as the engine's own fields with caller-sized arrays: every count is the total, a short array is filled as far as it fits and the call is REFUSED, and a real refusal has every count 0 - so the Zig side sizes, allocates exactly and reads once, and never loops a buffer up to a returned total"
    - "SaveDataResource writes under the data storage's own name - the shipped Data - so the composers write through the same tree serialiser to a path under the user RMG root instead, then read the file back through the storage and compare it before saying OK"
    - "The whole gesture set of a canvas is one state machine over tile coordinates (press / drag / release, y up), so the mouse path and a scripted rmgg_drag run the same code and the core tests cover it with no ImGui"
    - "A file-level undo that is not the map's: a composer document keeps clones of its own content (64 deep), a gesture adopts the clone it took when it began, a refused edit cancels its snapshot"

key-files:
  created:
    - Sources/editor/core/rmg.zig
    - Sources/editor/core/composers.zig
    - tools/zig/composer_roundtrip_test.cpp
  modified:
    - Sources/src/EditorBridge/bridge.h, bridge.cpp, session.h, session_rmg.cpp
    - Sources/editor/core/bridge.zig, editor.zig, fake_bridge.zig, root.zig
    - Sources/editor/app/c_bridge.zig, c_bridge_test.zig, commands.zig, panels.zig, panels_logic.zig, panels_m3.zig
    - tools/zig/editor_bridge_test.cpp
    - build.zig
    - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-PARITY.md, deferred-items.md

key-decisions:
  - "D-09 mount (costly, decided once): the RMG root is one storage layer, 'RMG_USER', mounted by BkEditorStart and swapped with the mod layer by BkEditorSetMod so that it always sits over Data and below the mod; the mod layer is taken off and put back above it, which is why the session keeps the mod's storage. The root follows the platform's user root (a cheap compare on every RMG entry, a remount only when it moved), so the engine tests that re-point the user root and a person who changes it see the right folder"
  - "Shipped is read-only in the bridge: a write whose name exists in a layer below the user's but not in the user's own is REFUSED with the Save-As message and writes nothing. The app learns it from that refusal (the Save button then asks for a name and the title says shipped) - there is no 'is this the user's file' read yet (deferred-items)"
  - "Bridge entry names: BkEditorRmgGraph was already 05-08's template-graph struct, and C has one identifier space, so the graph entries are BkEditorRmgReadGraph / BkEditorRmgWriteGraph on BkEditorRmgGraphRecord; the container ones follow (BkEditorRmgReadContainer / WriteContainer on BkEditorRmgContainerRecord)"
  - "Copy-in is bridge-side, not an app-side copy: BkEditorRmgImportPatch validates the source as a map the same reader Open uses, builds the destination from the user RMG root + the map's own season name + its bare file name, replaces only the user's own file and loads the copy back through the storage as a patch (a copy that does not load is removed). The app shows the YES/NO naming the destination first (dry run), then applies"
  - "Stored names in a graph are data: a node's container and a link's descriptor are kept as given and only a control character is refused, because shipped data holds a descriptor with its author's drive in front and links typed 2. The graph Check! reports a name that is not storage-relative and offers a strip; a container's patch names keep the strict rule (they are what the copy-in exists to fix)"
  - "Composer scope: one file per composer (D-08: the lists are folder scans), its own undo and dirty flag, never the map's history. New/Open on a dirty file asks 'Discard changes?' (the MFC saved silently)"
  - "The tri-state patch edit keeps the MFC's rule: a cell is on, off or mixed over the selection; mixed is 'unchanged' unless clicked, and an empty setting over a mixed selection leaves the setting alone"
  - "Check! never rewrites: it lists findings (errors and warnings) and each fix is a button or rmgc_fix / rmgg_fix; Fix all is one undo step. The graph's part count below 8 is the engine's own clamp (RMGeneration.cpp:1455) reported as an error with a set-to-8 fix - the MFC repaired it only when a descriptor happened to carry the storage prefix"
  - "Commands take names relative to the kind's own folder (common\\road_cross..., winter\\graph_escort2) because the auto grammar caps an argument at 64 characters and has no commas: fixed fields use name:field:value, a patch from the user's maps folder is rmgc_import:<map name>, and the canvas is rmgg_drag / rmgg_ctrl_drag with four tile numbers"
  - "Open Question 4 (one generic list+properties widget or five windows), decided here for 05-10 to inherit: the file row, the Open combo with its filter, the findings list, the Discard/Delete confirmations and the commands' name:field:value shape are shared helpers; the tables (container patches) and the canvas (graphs) are their own windows. 05-10 builds the fields and templates windows on the same helpers"

requirements-completed: [D-06, D-07, D-08, D-09, D-10, D-11, D-12, D-37, D-40]

duration: ~2h 30m wall (about 55 minutes of it the engine tier and the gates running)
completed: 2026-10-03T03:00:00Z

coverage:
  - deliverable: "D-09 user RMG root mounted over Data and below the mod; shipped files read-only; everything a user file references resolves through storage"
    verification:
      - kind: test
        ref: "editor_bridge_test.cpp#TestM3RmgContainers (editor-bridge: M3 rmg containers ok): a container written under a user name lands under <UserRoot>/rmg, reads back through the storage and loads in LoadDataResource (the game's reader), is listed by the folder scan, a write to a shipped name is REFUSED with 'Save As' and writes nothing, bad names (empty, .., rooted, drive, wrong folder, wildcard, empty component, too long) are BAD_ARGUMENT, and with EditorTestMod active the root moves to mods/EditorTestMod/rmg and leaves with the mod"
        status: pass
    human_judgment: false
  - deliverable: "D-06/D-07 Containers Composer reading and writing SRMContainer through the engine's serialisers; format fidelity"
    verification:
      - kind: test
        ref: "TestM3RmgContainers (two-pass read equals the engine's own load, capacity below the total writes what fits and says the total, an index outside the patches, a season outside 0..3, more than 512 patches and a patch outside the data are REFUSED writing nothing); core composers.zig tests; c_bridge_test 'M3 rmg composer reads round trip ok' (the Zig two-pass over the real C layout)"
        status: pass
      - kind: command
        ref: "zig build test-rmg-composer-roundtrip: 'composer-roundtrip: 404 containers, 102 graphs ok' - read, written, re-read equal, and a second write holding the same bytes"
        status: pass
    human_judgment: false
  - deliverable: "D-10 a patch outside the storages is copied into <rmg root>/Scenarios/Patches/<season>/, not refused"
    verification:
      - kind: test
        ref: "TestM3RmgContainers (dry run names the destination and copies nothing; apply copies, the copy loads as a patch and a container listing it writes; a non-map, a relative path and a bad flag are REFUSED/BAD_ARGUMENT; a shipped name is never replaced); core 'a patch outside the storages waits for its YES'"
        status: pass
      - kind: command
        ref: "map-editor-m3-auto 1088-1098 (rmgc_import:m3_auto_rmg, expect=rmgc_pending:1, shot m3-containers-copyin, rmgc_import_yes) - shot inspected: the popup names the file and the destination under the user RMG root"
        status: pass
    human_judgment: false
  - deliverable: "D-11 Graphs Composer canvas gestures, node and link properties; D-12 Check! for containers and graphs with explicit fixes"
    verification:
      - kind: test
        ref: "rmg.zig tests (add node with no overlap and a patch each way, move with the canvas clamp and overlap revert, resize with the one-patch minimum, Ctrl+drag link, hit testing with edge flags and links, delete node renumbering, set node container with the season rule and merged script lists, container and graph Check! findings and Fix all as one step, a drive in a stored name stripped on request); composers.zig tests; editor-bridge: M3 rmg graphs ok"
        status: pass
      - kind: command
        ref: "map-editor-m3-auto 1066-1210 (489 actions, rc 0): shipped container and graph open, Check! clean on the shipped container, a graph drawn by rmgg_drag / rmgg_ctrl_drag, rmgg_check -> expect=rmgg_errors:1 (shot m3-graphs-check inspected), rmgg_fix_all -> 0, saved under the user RMG root and reopened with the same nodes, links and part count"
        status: pass
    human_judgment: false
  - deliverable: "D-40.4 first half: every shipped container and graph loads, saves and reloads equal on the data-only tier"
    verification:
      - kind: command
        ref: "zig build test-rmg-composer-roundtrip -Dtarget=aarch64-macos: composer-roundtrip: 404 containers, 102 graphs ok / PASS"
        status: pass
    human_judgment: false
  - deliverable: "D-40.8 guard: composer writes are file-level; the existing tiers stay green"
    verification:
      - kind: command
        ref: "zig build test: 32/32 steps, 724/724 tests; test-editor-bridge full tier rc 0 with no FAIL; test-map-editor-engine PASS (260 objects); map-editor-smoke PASS; map-editor-game-reads-it-m3 PASS; test-random-missions only=kharkov42: 12 cases, 0 failed; test-rmg-determinism PASS; zig test tools/zig/build_hermeticity_test.zig: 3 tests passed"
        status: pass
    human_judgment: false
  - deliverable: "PARITY rows R1-R6, R14 closed with evidence"
    verification:
      - kind: command
        ref: "05-PARITY.md Evidence cells filled; grep of Sources/editor and Sources/src/EditorBridge finds Editor\\Default*.xml only in comments"
        status: pass
    human_judgment: false
  - deliverable: "Windows leg (D-37) and the composer frames on win-home"
    verification:
      - kind: command
        ref: "win-home (x86_64-windows-msvc, branch head 58f440dcd, no GUI): test-editor-core 361/361 passed, the app tiers (panels, auto, testlaunch) 211/213 passed with the 2 usual skips, test-editor-bridge and test-rmg-composer-roundtrip COMPILE under MSVC, install-map-editor builds the editor app. Pending: the engine tiers' RUN and the composer frames of map-editor-m3-auto (a GUI cannot be launched over ssh) and the data-only step in CI (it is not in cross-platform.yml yet - deferred-items.md From 05-09)"
        status: pending
    human_judgment: true
    rationale: "win-home runs no GUI over SSH (executor rules), and wiring test-rmg-composer-roundtrip / test-rmg-determinism into the five engine-C++ targets is a workflow edit left to the orchestrator; the new C++ compiles under MSVC on win-home and is platform-plain (std::filesystem, the engine's backslash form, no std::min/max, no reserved identifiers) and the Zig has no platform code."

# Phase 05 Plan 09: Containers and Graphs composers and the user RMG root - Summary

One-liner: the Containers and Graphs Composers - dockable Tools windows with the MFC's columns, a draw-list canvas that runs the MFC's
gestures as one state machine, patches copied in rather than refused, and Check! findings with explicit one-undo-step fixes - read and
write the engine's own SRMContainer / SRMGraph through a user RMG storage root mounted over Data and below the mod, and all 404 shipped
containers and 102 shipped graphs round-trip equal, byte-stable on a second write.

## Accomplishments

- **Task 1 (`4c6bfc923`):** the "RMG_USER" mount in BkEditorStart / BkEditorSetMod (re-pointed when the user root moves), the record
  entries and their two-pass arrays, the shipped read-only rule, the patch info and copy-in entries, `rmg.zig`, `composers.zig`, the
  Editor's read/write/import/source methods, both vtable implementations (the C adapters and the fake) in the one commit,
  `TestM3RmgContainers`, `TestM3RmgGraphs`, the c_bridge composer reads.
- **Task 2 (`8475209ed`):** the Containers Composer window (twelve container columns, seven patch columns, File row with the scan-backed Open
  combo, the picker with multi-select and Browse, the copy-in popup, the properties popup with tri-state cells, the right-click menu and
  the Insert/Delete/Space keys, Check! with Fix / Fix all) and - because the files are shared - the Graphs Composer window with it:
  the canvas, the node and link dialogs, the graph's eleven columns. The `rmgc_*` / `rmgg_*` commands and predicates; `panels_logic`
  canvas geometry, list columns, copy-in destination and patch-browse rules with tests.
- **Task 3 (`a404310d7`):** `composer_roundtrip_test.cpp` and `test-rmg-composer-roundtrip`, the m3-auto frames 1066-1214 (six shots, all
  inspected), PARITY R1-R6 and R14, deferred items. The round trip found two things a hand test would not (below).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] BkEditorListRmg's graphs folder was wrong**
- **Found during:** Task 1's graph test (the scan listed no graphs)
- **Issue:** `ListRmgFolder` kind 2 walked `graphs\`; the shipped graphs live under `scenarios\graphs\` (as the templates' own Graphs lists name them). 05-03 only ever used kinds 0, 1, 4, 5.
- **Fix:** the folder is `scenarios\graphs\`; the bridge test lists 102, the c_bridge test lists them through the ABI.
- **Files modified:** Sources/src/EditorBridge/session_rmg.cpp
- **Commit:** 4c6bfc923

**2. [Rule 2 - Missing critical functionality] SaveDataResource cannot be the composers' writer**
- **Found during:** Task 1, reading Resource_Types.h before writing
- **Issue:** the plan names SaveDataResource, which opens `pDataStorage->GetName() + name + ".xml"` - the installation's Data folder. A composer Save through it would write into shipped data (T-05-09-02).
- **Fix:** the composers write through the same tree serialiser (`CreateDataTreeSaver`, the structs' own `operator&` under the same RMGC labels) to a path under the user RMG root, create the folders there, and read every write back through the storage and compare it before OK.
- **Files modified:** Sources/src/EditorBridge/session_rmg.cpp
- **Commit:** 4c6bfc923

**3. [Rule 1 - Bug] Writes refused data the shipped files hold**
- **Found during:** Task 3's first round-trip run (94 of 102 graphs)
- **Issue:** the bridge's write refused a link typed 2 (seven shipped graphs; the generator treats every non-road type as a river) and a descriptor with its author's drive in front (`c:\a7\data\terrain\...`, winter/graph05). "Every shipped file loads, saves and reloads equal" (D-07) does not hold if the writer is stricter than the data.
- **Fix:** stored names in a graph are data - only a control character is refused - and a link type is bounded 0..255; the graph Check! reports a name that is not storage-relative with a strip fix and a non-road/river type as a warning. The record rules for a container's patch names stay strict (a patch outside the data is what the copy-in is for).
- **Files modified:** Sources/src/EditorBridge/session_rmg.cpp, bridge.h, Sources/editor/core/rmg.zig, fake_bridge.zig, tools/zig/editor_bridge_test.cpp
- **Commit:** a404310d7

### Differences from the plan's text (recorded, not bugs)

- **Entry and function names.** `BkEditorRmgGraph` was already 05-08's template-graph struct (one C identifier space), so the graph entries are `BkEditorRmgReadGraph` / `BkEditorRmgWriteGraph` on `BkEditorRmgGraphRecord`; the container ones are `BkEditorRmgReadContainer` / `WriteContainer`. The C++ side's `ReadRmgContainer` / `WriteRmgContainer` of the plan are `ReadRmgContainerRecord` / `WriteRmgContainerRecord` (the record is the ABI struct, so the bridge entries stay thin).
- **Copy-in is bridge-side** (the plan said an app-side file copy after the bridge validates): the bridge owns the user RMG root, so it builds the destination and does the copy; the app asks first with a dry run and shows the YES/NO naming the destination.
- **`rmgc_patch_set` grammar.** The auto grammar has no commas, so the command is `<index>:<field>:<value>` (place, north, east, south, west) instead of `<index>,<spec>`; commands take names relative to the kind's folder (64-character cap).
- **Task commits.** Tasks 2 and 3 share panels_m3.zig, commands.zig and panels.zig, so the Graphs Composer's window and canvas landed with Task 2's commit; Task 3's commit holds the round trip, the frames, the PARITY rows and the bridge/Check! changes the round trip forced. The first commit was amended once, before any push, to carry composers.zig that its root.zig already imported.
- **One gate run was spoiled and rerun whole.** Two background runs of the engine tier overlapped on `zig-out/local-test` (the second started while the first still ran) and tripped each other's scratch files (about 30 spurious FAIL lines in the layer and cascade tests and the seeded regeneration); the tier was rerun alone, rc 0, no FAIL. No test was changed for it.
- **Windows:** the non-GUI tiers and the MSVC compile ran on win-home after the push (core 361/361, app tiers 211/213, the engine tier and the round trip compile, the editor app builds); the engine tiers' run and the composer frames need CI or a hand run - see the coverage entry.

**Total deviations:** 3 auto-fixed (2 bugs, 1 missing critical functionality), the rest recorded. **Impact:** none on the plan's contract; the data-only round trip is stricter than the plan's "load, save, reload equal" (a second write is byte-identical).

## Issues Encountered

- Zig 0.16 stumbles, each one build: `std.mem.trimRight` is `trimEnd`; a `{d:>4}` on a signed integer prints a `+` (the size cell takes unsigned); `catch ""` and `catch below[0..0]` have incompatible types next to `bufPrint`; `for (...) |x| if (c) {...};` after an `else` takes no semicolon; a function parameter may not shadow the fake bridge's `record` method.
- The C++ engine tier takes about four and a half minutes per cold build and about ten with every test; the whole gate list above ran in one background script in sequence.

## Deferred Issues

See `deferred-items.md` ("From 05-09"): one file per Browse, Check! reading every patch on the main thread, Save learning a file is shipped from the refusal, one file per composer, the user root's place below the mod layer, no script named by the composers, the CI wiring of the two data-only steps and the Windows GUI legs.

## Known Stubs

None. The Graphs Composer's "Supported Settings" column is computed (SRMGraph::GetSupportedSettings ported), the Containers one too.

## Threat Flags

None beyond the plan's register. T-05-09-01 (traversal through names): the record name is lower-cased, must start under its folder, pass IsRelativeDataName and hold no wildcard or control character - ten bad names are BAD_ARGUMENT and write nothing (engine test). T-05-09-02 (writes into shipped data): writes go only to the mounted user root, a shipped name is REFUSED (tested), and the engine test asserts nothing appeared under the installation's Data. T-05-09-03 (copy-in): the source must read as a map with the Open reader, the destination is the user root + Scenarios/Patches + the map's own season name + its bare file name, and a copy that does not load back is removed. T-05-09-04 (oversize): 512 patches, 256 nodes, 1024 links, 4096 IDs, 512 areas are REFUSED above (tested for patches and nodes), and the two-pass reads size to the totals.

## Gate results (macOS arm64)

- `zig build test -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run --summary all`: `Build Summary: 32/32 steps succeeded; 724/724 tests passed`.
- `zig build test-editor-bridge test-map-editor-engine ...`: `editor-bridge: M3 rmg containers ok`, `editor-bridge: M3 rmg graphs ok`, `editor-bridge: PASS`, `map-editor-engine: M3 rmg composer reads round trip ok`, `map-editor-engine: PASS (260 objects)`, rc 0, no FAIL.
- `zig build map-editor-m3-auto ...`: `BK_EDITOR_AUTO: done (489 actions)`, rc 0 (M1, M2 and M3 scenarios).
- `zig build test-rmg-composer-roundtrip test-rmg-determinism ...`: `composer-roundtrip: 404 containers, 102 graphs ok`, `composer-roundtrip: PASS`, `rmg-determinism: byte identical ok` x2, `rmg-determinism: PASS`.
- `zig build test-editor-core test-map-editor-panels map-editor-smoke ...`: green; `map-editor: smoke PASS (52 steps, 260 objects, ...)`.
- `zig build map-editor-game-reads-it-m3 ...`: `game reads it M3 PASS`, rc 0.
- `zig build test-random-missions ... -Drandom-missions-sweep=only=kharkov42`: `random-missions: 12 cases, 0 failed` (the mount did not disturb the game's own generation path).
- `zig test tools/zig/build_hermeticity_test.zig`: `All 3 tests passed` (after the build.zig edit).

## Self-Check: PASSED

- Files: Sources/editor/core/rmg.zig, Sources/editor/core/composers.zig, tools/zig/composer_roundtrip_test.cpp, the six shots under zig-out/local-test/map-editor-m3-auto (m3-containers-shipped / -copyin / -user, m3-graphs-shipped / -check / -user), and 05-09-SUMMARY.md exist.
- Acceptance greps: `ReadRmgContainerRecord|WriteRmgContainerRecord` in session_rmg.cpp, `BkEditorRmgContainerRecord|BkEditorRmgWriteContainer` and `BkEditorRmgReadGraph|BkEditorRmgWriteGraph` in bridge.h, `pub const Container` and `pub const Graph` in rmg.zig (imported from root.zig), `MountRmgRoot` in bridge.cpp's start and set-mod paths with the D-09 costly rating in the bridge.h comment, `drawContainersComposer` and `drawGraphsComposer` in panels_m3.zig, `rmgc_` on 28 lines of commands.zig, `copyInDestination` with its test in panels_logic.zig, `test-rmg-composer-roundtrip` in build.zig; zero `std::min`/`std::max` in the added C++.
- Commits: 4c6bfc923, 8475209ed, a404310d7 are in `git log`.

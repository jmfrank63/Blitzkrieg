---
phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
fixed_at: 2026-09-30T12:30:00Z
review_path: .planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-REVIEW.md
iteration: 1
findings_in_scope: 30
fixed: 30
skipped: 0
status: all_fixed
---

# Phase 4: Code Review Fix Report

**Fixed at:** 2026-09-30
**Source review:** `.planning/phases/04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts/04-REVIEW.md`
**Iteration:** 1

**Summary:**
- Findings in scope: 30 (3 Critical, 27 Warning). CR-B01 and CR-C01 are one defect with one fix and one commit.
- Fixed: 30
- Skipped: 0
- Info (outside the scope; fixed only where trivial and safe): 9 of 20 fixed, 2 of those only in part, 11 left as they are.

Every finding was checked against the source before it was changed. None turned out wrong. Where the review's suggested fix did not fit, the change was adapted and the reason is given below: WR-A02, WR-B01, WR-B02, WR-B04, WR-C06 and WR-C09.

## Verification

All runs were in the main checkout (`.worktrees/map-editor-6`, `workflow.use_worktrees: false`) on macOS arm64. The logs are in `zig-out/local-test/04-fix-*.log`.

| Tier | Result |
|------|--------|
| `test-editor-core test-map-editor-panels test-map-editor-view test-map-editor-auto test-map-editor-testlaunch` | 519/519 tests passed (`04-fix-tier1.log`) |
| `test-map-files test-editor-bridge test-map-editor-engine` | `map-file: PASS`, `editor-bridge: PASS`, `map-editor-engine: PASS (260 objects)` (`04-fix-tier2.log`) |
| `map-editor-host-check map-editor-smoke map-editor-auto map-editor-auto-m2 map-editor-game-reads-it-m2` | Everything passed, and `BK_EDITOR_AUTO: done (298 actions)`. The game-reads-it M2 run now reports `script m2_script ran (loaded=1 init=1)` from the real `lua_call` result (WR-A06) (`04-fix-tier3.log`). No run flaked, so nothing was run again. |
| `test-map-files-m2-sweep test-editor-bridge-m2-sweep` (local, with the new per-kind minimums) | 59 maps and 460 edits, and 57 maps and 238 edits, all restored byte-exact (`04-fix-wrc05-sweeps.log`) |
| CI `Cross-platform validation` on `feat/map-editor-m2` | Run **36713320474** passed on all six jobs (windows-platform, windows-mingw-platform, macos-platform, macos-intel-platform, linux-platform, linux-arm-platform). Two earlier runs failed and were fixed; see the next section. |

`build.zig` was not edited, so the hermeticity test was not needed.

### CI failures and their fixes

**Run 36708663502** failed on two jobs:
- **windows-platform:** the WR-A09 engine test named a variable `small`, which is a macro in the Windows SDK (`rpcndr.h`). Renamed in bdc0a7d84.
- **macos-platform:** `TestM2ScriptIDs` compared a save of its second open of coldwinter with the save of its first. `SVertexAltitude` padding goes to the file raw, so two opens can differ. The new tests before it changed the heap layout, and the runner hit the difference. This test weakness existed before this fix pass. The test now compares saves of one open only (4a2811870).

**Run 36710555563** failed on windows-platform. The debug engine asserted `Repeated link` (`LinkObject.cpp:104`) on the WR-A02 test map, where two bridges entries share a span. `BuildOneBridge` now skips a link ID the session already holds, and the span count at open counts unique link IDs across all entries (d189039ee).

After these fixes, `test-editor-bridge` and `test-map-editor-engine` were run again locally (`04-fix-ci1.log`, `04-fix-ci2.log`), and CI run 36713320474 passed.

## Fixed Issues

### CR-A01: Resampling a road or river whose record has fewer than 2 control points reads out of bounds

**Files modified:** `Sources/src/EditorBridge/session_vso.cpp`, `tools/zig/editor_bridge_test.cpp`
**Commit:** 15911e565
**Applied fix:**
- `ReplaceVsoInSession` now checks `LongEnough` on the record as read, before `modify()` runs. A record with fewer than 2 control or sampled points is refused as "kept as read".
- This covers move, width, opacity, insert and point delete.
- Engine test: a coldwinter copy with a 1-point road and a 0-point road. Every edit is refused, the map saves byte-identical, and a whole-record delete and its undo still work.
- **Found on the way:** a railroad with fewer than 2 control points crashes the AI's `CRailroad` graph at open. The test therefore uses a non-rail type. This is a latent engine/game issue with malformed files and is not something the editor introduced.

### CR-B01 / CR-C01: Save As copy-along silently overwrites a different script beside the new map

**Files modified:** `Sources/editor/core/script_file.zig`, `Sources/editor/app/panels.zig`, `Sources/editor/app/panels_m2.zig`
**Commit:** 7854d9b33
**Applied fix:**
- **Core guard:** `copyAlong` now follows `copyInto`'s contract. A new `CopyAlongOutcome` returns `.exists` when a different `<name>.lua` is already beside the new map, unless `overwrite` is set.
- **UI:** when that file is there, `offerScriptCopyAlong` changes its question to "A different <name>.lua is already beside the new map. Replace it with the old map's?" with the buttons **Replace** and **Keep it**. Only Replace overwrites.
- A file that appears after a plain "Copy?" is not replaced. The question is asked again as "Replace it?", and the command answers refused.
- Core tests cover both answers.

### WR-A01: `SGroupEdit::Revert` has no rollback

**Files modified:** `Sources/src/EditorBridge/session_groups.cpp`
**Commit:** 353840f96
**Applied fix:** `Revert` now mirrors `Apply`. If `AddGroup(oldGroup)` fails after the new group came out, the new group is put back. The message asks for a reopen only if that also fails.
**Status:** fixed, requires human verification. The failure path cannot be reached on shipped data, so there is no regression test.

### WR-A02: Group removal touches the engine before the map decides; shared or duplicated link IDs

**Files modified:** `Sources/src/EditorBridge/session_groups.cpp`, `Sources/src/EditorBridge/session.cpp`, `tools/zig/editor_bridge_test.cpp`
**Commits:** d721357b7; follow-up d189039ee (a span named by two entries is placed once, for the Windows debug engine's `Repeated link` assert)
**Applied fix:**
- **Order in `RemoveGroup`:** every span now leaves both map copies first, and only then the engine. If the map refuses a span, the spans already taken and the entry are restored, and the engine is untouched.
- **Why not the review's dry run:** it proposed a `WhyRefused` dry run, but that function is internal to MapOverlay. Doing the real map removal first gives the same guarantee.
- **`CanTakeOutWhole`:** now refuses a link ID that the group repeats, or that any other bridges or entrenchments entry names.
- **`BuildOneBridge`:** places a link ID only once per entry. The open counts such a span once, so `bridge_span_count` still equals `bridge_span_placed`.
- **Engine test:** arnheim with a shared span and a repeated span. Delete and rotate are refused, every bridge link is still in the engine, and the map saves byte-identical.

### WR-A03: `SetSessionAIGeneralSide` silently drops non-empty sides when a put shrinks the side count

**Files modified:** `Sources/src/EditorBridge/session_records.cpp`, `Sources/editor/core/fake_bridge.zig`, `tools/zig/editor_bridge_test.cpp`
**Commit:** 0013b32dc
**Applied fix:**
- A put is refused when it would drop a side, other than the side it names, that holds parcels or mobile IDs.
- The undo of a put that created sides still works, because those sides are empty.
- The fake bridge has the same rule. Engine test on coldwinter.

### WR-A04: Script areas have no "file's own data" undo exemption

**Files modified:** `Sources/src/EditorBridge/session.h`, `session.cpp`, `session_records.cpp`, `Sources/editor/core/fake_bridge.zig`, `Sources/editor/core/editor.zig`, `tools/zig/editor_bridge_test.cpp`
**Commit:** 607bf0bd0
**Applied fix:**
- The session keeps `openedAreas`. A put that matches one of them bit for bit skips the name-present, size and centre rules. The duplicate-name rule still applies to it.
- The fake bridge does the same.
- Core and engine tests delete, move and undo an off-map area with a negative size, and check that a new odd area is still refused.

### WR-A05: Setting a script ID edits objects of unknown type

**Files modified:** `Sources/src/EditorBridge/session.cpp`, `Sources/editor/core/fake_bridge.zig`, `Sources/editor/core/editor.zig`, `tools/zig/editor_bridge_test.cpp`
**Commit:** 123822dfa
**Applied fix:** `SetSessionObjectScriptID` refuses a link ID listed in `unknownLinkIDs`, and so does the fake. Covered by a core test and by the engine test `TestUnknownObjectIsReadOnly`.

### WR-A06: `BK_MAP_TRACE` script line reports `init=1` whenever the file loaded

**Files modified:** `Sources/src/AILogic/Scripts/Scripts.cpp`
**Commit:** 1c5588da4
**Applied fix:** `init=` now reports `lua_call`'s own return value (0 means Init ran).
**Status:** fixed, requires human verification of the failure branch. `map-editor-game-reads-it-m2` and `map-editor-auto-m2` confirm the success path (`init=1`). No test yet runs a script whose Init errors or is missing, which should report `init=0`.

### WR-A07: The bridge build toggle applies partially with no rollback and ignores shared link IDs

**Files modified:** `Sources/src/EditorBridge/session_groups.cpp`, `tools/zig/editor_bridge_test.cpp`
**Commit:** 4626e38b5
**Applied fix:**
- `SBridgeBuildEdit::Put` checks every span (non-zero link ID, present in the map) before writing any HP. If a write still fails, it puts back the HPs already written.
- The toggle now runs `CanTakeOutWhole`.
- Engine test: a WoodenBig bridge whose span a second entry also names. The toggle is refused and the map is unchanged.

### WR-A08: Bridge stats checked only for the first begin and first line

**Files modified:** `Sources/src/EditorBridge/session_groups.cpp`, `tools/zig/editor_bridge_test.cpp`
**Commit:** 14a624c3e
**Applied fix:**
- `BridgePlanInputFor` now checks every entry of `begins`, `lines` and `ends` against the spans.
- It also checks every span's slab and girders against the segments.
- Engine test: all 18 shipped bridge types still plan.

### WR-A09: Float-to-int conversions of unbounded ABI coordinates are undefined behaviour

**Files modified:** `Sources/src/MapFile/MapGeometry.cpp`, `Sources/src/EditorBridge/bridge.cpp`, `bridge.h`, `tools/zig/map_file_test.cpp`, `tools/zig/editor_bridge_test.cpp`
**Commits:** 824da694c; follow-up bdc0a7d84 (the test's variable `small` renamed for the Windows SDK)
**Applied fix:**
- **`PlanBridge`:** refuses a drag or begin origin beyond ±1e6 world units, and range-checks the span count before converting it. The UB sanitizer in the map-file tier showed that `FitVisOrigin2AIGrid`'s `Vis2AI` also needs the bound.
- **`VisLengthToAI`:** clamps to ±1e9 AI units, NaN included.
- **ABI:** bridge and fence drags and the three area gestures take coordinates only within ±1e6; beyond that they return `BAD_ARGUMENT`.
- Map-file and engine tests cover each entry point.

### WR-A10: Assigning a group script ID does not warn when a start command or reserve position names the object

**Files modified:** `Sources/src/EditorBridge/session.h`, `session.cpp`, `session_records.cpp`, `Sources/editor/core/editor.zig`, `tools/zig/editor_bridge_test.cpp`
**Commit:** c827835e2
**Applied fix:**
- A new `GroupHoldWarning` checks start command units, and reserve position guns and trucks, against the groups.
- `SetSessionGroup` and `SetSessionObjectScriptID` answer OK with that note. The core shows it after adding a script ID to a group or setting an object's script ID.
- The engine test covers both orders.

### WR-B01: A failed replay of a bridge-logged edit wedges the undo stack

**Files modified:** `Sources/editor/core/editor.zig`, `fake_bridge.zig`, `tools_vso.zig`, `tools_groups.zig`
**Commit:** d4042836c
**Applied fix:**
- **Partial replay:** when a token fails part-way, the tokens already replayed are put back, so a retry starts where the first attempt did.
- **Unwinding also fails:** `replay_broken` then refuses every further undo and redo with a single message until the map is reopened.
- **Objects re-read:** the re-read after an objects-scope edit (`afterReplay`) now runs after the entry has moved stacks. A failed re-read therefore leaves the history where the bridge is.
- **Forward edits:** a failed re-read is reported on the status line. Pre-sizing the list is not possible, because the new object count is only known after the edit.
- The fake gains per-token failure hooks. Core tests cover both directions, the broken state and a failed re-read.

### WR-B02: Tools keep a bare list index as their selection and never revalidate it after undo or redo

**Files modified:** `Sources/editor/core/tools.zig`, `tools_groups.zig`, `tools_vso.zig`, `tools_ai.zig`, `Sources/editor/app/view.zig`
**Commit:** 4d5b7f9b3
**Applied fix:**
- **Capture and resolve:** `View.runUndoable` asks every index-selecting tool to capture its selection before the undo or redo and resolve it after.
- **What identifies an item:**
  - a road by its saved nID;
  - a bridge, trench, area or reserve position by its record;
  - an AI parcel by its own fields, and its point by its own fields.
- **Adapted from the review:** a replay that keeps the list's length keeps the index. Without that, a rotate or toggle undo/redo lost the bridge selection, and `map-editor-auto-m2` failed at `expect=bridge_built`.
- A road keeps its last grab while that point still exists.
- Core tests for bridges, areas and roads.

### WR-B03: Undo of a camera-anchor edit is refused when the file's original anchor is off the map

**Files modified:** `Sources/src/EditorBridge/session.h`, `session.cpp`, `session_records.cpp`, `Sources/editor/core/fake_bridge.zig`, `editor.zig`, `tools/zig/editor_bridge_test.cpp`
**Commit:** c4799f30f
**Applied fix:** the anchors as they were at open are now exempt when a put sets a slot back to exactly that value. This is done in both the real bridge and the fake. Core and engine tests.

### WR-B04: "Open script" hands a map-controlled `.lua` to the shell's default verb

**Files modified:** `Sources/editor/core/script_file.zig`, `Sources/editor/app/panels.zig`, `panels_m2.zig`, `commands.zig`
**Commit:** 2b297a94a
**Applied fix:**
- Of the review's options, this uses "reveal the folder": the button is now **Open script folder**.
- `script_file.folderUrl` builds the URL of the resolved folder. It still requires the script to be there, and it never names the `.lua` itself.
- The core test checks the URL never contains `.lua`.
**Status:** fixed, requires human verification of the folder opening on Windows. The behaviour of `SDL_OpenURL` on a folder URL was not run there.

### WR-B05: No shipped-folder guard in the script file writers

**Files modified:** `Sources/editor/core/script_file.zig`, `Sources/editor/app/panels.zig`, `panels_logic.zig`, `game_reads_m2.zig`
**Commit:** 1840be8da
**Applied fix:**
- `copyInto`, `copyAlong` and `copyForTest` take the base root and answer `.shipped` for a destination that `shipped.isShipped` classifies as shipped.
- Callers report it on the status line.
- The core test covers the installation's own Data and another installation's Data.

### WR-B06: Fake bridge diverges from the real bridge

**Files modified:** `Sources/editor/core/fake_bridge.zig`, `files.zig`, `script_file.zig`, `editor.zig`, `tools_vso.zig`, `tools_groups.zig`
**Commit:** 2e10fa46b
**Applied fix:** the four differences the review listed are now closed:
1. `readVso` returns the full saved name (season folder plus `Roads3D\`/`Rivers\` plus name).
2. The capacity is 256 points, spans and pieces, the same as the tools' limits.
3. A group put exempts the groups as they were at open.
4. `FakeFiles.list(".")` lists the entries with no folder.

Core tests for each.

### WR-B07: A double click with a road selected and nothing being drawn deselects it

**Files modified:** `Sources/editor/core/tools_vso.zig`
**Commit:** eb9e1079b
**Applied fix:** `.double_click` now only finishes a line. With nothing being drawn it does nothing, so the selection and last grab stay. Enter and Space still deselect. Core test.

### WR-B08: `AIGeneral.deleteSelected` deletes the whole parcel when the point index is stale

**Files modified:** `Sources/editor/core/tools_ai.zig`
**Commit:** cbd16475a
**Applied fix:** a stale point index now clears the point selection and shows "that point is gone". It no longer removes the parcel. Core test.

### WR-C01: `errdefer` in functions that return `Status` never runs

**Files modified:** `Sources/editor/app/c_bridge.zig`
**Commit:** 4dd494004
**Applied fix:** `readStartCommand`, `readAiSide`, `readGroup` and `recordKeys(.group)` now free their allocations through a handed-over flag in a `defer`.
**Status:** fixed, requires human verification. No test forces these particular failure paths.

### WR-C02: Two-pass reads treat "no map is open" as a successful sizing pass

**Files modified:** `Sources/editor/app/c_bridge.zig`, `c_bridge_test.zig`
**Commit:** 64a7a0d33
**Applied fix:**
- Each count is pre-set to a sentinel value. After a REFUSED that did not overwrite it, the refusal is passed on instead of being taken as the sizing pass.
- This applies to start commands, AI sides, groups, the group insert probe, and the area, side and group key listings.
- The engine-tier test with no map open checks every one of them.

### WR-C03: The double-click routing drops the second of two fast clicks

**Files modified:** `Sources/editor/app/view_math.zig`, `Sources/editor/app/view.zig`
**Commit:** 868d8e382
**Applied fix:**
- `kindOf` takes the tool's `needs_double_click`. A tool without it gets `clicks == 2` as an ordinary press and release.
- Two view tests that encoded the old swallowing were updated to the new behaviour.
- A view-math test was added.

### WR-C04: `do=script_choose` reports OK when the copy was refused

**Files modified:** `Sources/editor/app/panels.zig`, `Sources/editor/app/commands.zig`
**Commit:** 0e3a4832b
**Applied fix:** `pickScript` returns `chosen`, `asked` or `refused`. `scriptChoose` and `script_overwrite_yes` answer from that value, not from the map's state afterwards.
**Status:** fixed, requires human verification. `map-editor-auto-m2` passes on the OK path. The refused path has no scenario step.

### WR-C05: The M2 sweeps can pass without exercising the edits

**Files modified:** `tools/zig/editor_bridge_test.cpp`, `tools/zig/map_file_test.cpp`
**Commit:** baffe1895
**Applied fix:**
- Both sweeps now require a minimum per kind, about half of today's counts.
- Engine sweep: bridge, fence and trench drawn 28 each, shipped bridge deleted 10, shipped entrenchment deleted 3, cascade delete 18.
- Map-file sweep: all 11 kinds.
- Both sweeps pass.

### WR-C06: `expect=test_game_script` can pass on a stale script copied by an earlier run

**Files modified:** `Sources/editor/core/script_file.zig`
**Commit:** 98bc501f7
**Applied fix:**
- The problem is fixed at the source rather than in the scenario. `copyForTest` deletes `<name>.lua` in the test folder whenever it makes no copy (missing or failed).
- The test game therefore runs this map's script or none. That also covers the predicate.
- Core test.

### WR-C07: `game_reads_m2.run` asserts nothing about fences; the shot may be stale

**Files modified:** `Sources/editor/app/game_reads_m2.zig`
**Commit:** 6883aac17
**Applied fix:**
- `play()` removes every `autoshot_*.rgba` before each run.
- The PASS line and its comment now say the fences were placed by the editor and loaded by a clean run, and are not in the game's trace.
- `map-editor-game-reads-it-m2` passes.

### WR-C08: `State.deinit` never frees the `ai_sides` list itself

**Files modified:** `Sources/editor/app/panels.zig`
**Commit:** 8ac7b10c9
**Applied fix:** added `self.ai_sides.deinit(self.allocator)` after `freeAiSides()`.

### WR-C09: `c_bridge_test` passes vacuously and some assertions are weaker than their comments

**Files modified:** `Sources/editor/app/c_bridge_test.zig`
**Commit:** 07f94a170
**Applied fix:**
- **No GPU:** the test now returns `error.SkipZigTest`, which the runner reports as a skip.
- **Why not the review's build-step rule:** it proposed requiring at least one executed engine test in the build step. That would fail GPU-less CI runners, so the visible skip was chosen instead.
- **Group-IDs sizing:** must be OK or REFUSED with a count.
- **Cascade delete:** must succeed, so its checks always run.
- **AI read:** accepts REFUSED only as the sizing answer for the mobile IDs or points it left out, checks the parcel count, and checks the index before reading.

## Info findings (outside the scope)

Fixed because each was trivial and safe:

| ID | Commit | What |
|----|--------|------|
| IN-B02 | 89c93089a | `IsBareScriptName` and `isBareName` both refuse Windows device names (CON, PRN, AUX, NUL, COM1-9, LPT1-9). Both tests carry the same cases. |
| IN-B03 | 863d466b0 | Negative index after OK is `error.Failed`, and `reloadObjects` truncates to the second pass's total. Deduplicating the read with `Document.reload` is not done. |
| IN-B04 | 593d7f12c | `open` and `close` bump every generation, including `record_generations` and `sounds_generation`. Core test. |
| IN-B06 | 67e94bbf4 | A refused delete keeps the selection in the Bridge, Entrenchment, Script Areas and Reserve Positions tools. |
| IN-B08 | 41b37e634 | `ensureSideExists` guards a negative side. The fake's float conversions are not changed. |
| IN-C03 | 8006cb29f | Moved the doc comment. The 256-point marker buffer is not changed. |
| IN-C04 | 46f578d54 | `showMap` and `closeMap` clear `scripted_buttons`. |
| IN-C05 | 4ab694333 | `chooseOtherScript` checks `os_dialogs` before taking the slot. |
| IN-C06 | e0ed3e8c0 | Documented that a (0,0) target reads as "none". |

Left as they are, because each is a refactor, a policy decision or trace-format work rather than a trivial fix: IN-A01 (shared `IsMapTraceOn`), IN-A02 (escaping in trace lines), IN-A03 (trace counts of held units and launched commands), IN-A04 (INT_MAX in the ID helpers), IN-A05 (cascade note for group deletes), IN-A06 (reserving log capacity before applying), IN-B01 (root directories in `listBeside`/`folderUrl`), IN-B05 (coalescing VSO drag tokens), IN-B07 (Esc policy during drags), IN-C01 (app-level selection indices after delete or undo), IN-C02 (the 12-side radio limit).

---

_Fixed: 2026-09-30_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 1_

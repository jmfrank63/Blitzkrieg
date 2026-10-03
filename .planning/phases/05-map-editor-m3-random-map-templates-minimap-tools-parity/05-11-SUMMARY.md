---
phase: 05-map-editor-m3-random-map-templates-minimap-tools-parity
plan: 11
subsystem: map-editor
tags: [map-editor, m3, app-shell, options, view-menu, drag-drop, single-instance, parity-closure, gates, hand-try-prep, checkpoint]
status: partial
stopped_at: "Task 4 (checkpoint:decision, the one-way deletion of the MFC editor): tasks 1-3 done, task 5 NOT started, nothing deleted"

requires:
  - phase: 05-map-editor-m3-random-map-templates-minimap-tools-parity
    plan: "01..10"
    provides: every M3 feature the parity rows point at; the m3-auto scenario and the engine, bridge, round-trip and determinism tiers
provides:
  - Tools > Options (game parameters into Test in game, default save format), the View menu over every docked panel and floating window with Reset layout, drag-drop open, Help (Keys and tools, F1) and About, ImGui layout persistence in layout.ini
  - single_instance.zig - a per-user stream socket that hands a second launch's map to the running editor
  - 05-PARITY.md closed (152 rows, none empty, 18 NF rows re-read against the MFC source), the recorded gate results, the spec marked, the hand-try checklist
  - three parity gaps found while closing the rows and fixed (Place tool ghost, red refused heights on the Heights minimap, the direction wheel's whole dial)
affects: [05-11 task 4 and 5 (the deletion), the phase 5 verification]

key-files:
  created:
    - Sources/editor/app/single_instance.zig
  modified:
    - Sources/editor/app/main.zig, panels.zig, panels_m3.zig, panels_logic.zig, commands.zig, testlaunch.zig, markers.zig, smoke.zig, view_math.zig, c_bridge_test.zig
    - Sources/editor/core/settings.zig
    - Sources/editor/imgui/imgui_backend.cpp, imgui_backend.h
    - tools/zig/editor_bridge_test.cpp, build.zig
    - docs/superpowers/specs/2026-09-19-portable-map-editor-design.md
    - .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-PARITY.md, 05-VALIDATION.md, deferred-items.md

key-decisions:
  - "The single-instance socket is a stream socket on every system (a Unix domain socket; on Windows AF_UNIX through the std), not a named pipe on Windows: one implementation, one test set that runs on every target; build.zig now names Windows 10 1803 as the oldest Windows a Windows target builds for, because the std hides net.UnixAddress behind that version"
  - "The second launch does its whole hand-off on a worker thread and gives up after 1.5 s; the owner serves each connection on its own thread (four at most): the std has no connect or read timeout and on Windows a shutdown does not wake a read on a hung owner's socket"
  - "Docking nodes are not switched on: the docked columns are fixed and the other windows float; View hides and shows each and Reset layout puts them back (PARITY V3)"
  - "The Place tool's ghost is the object's palette picture at half opacity over a ring, not the engine's sprite (PARITY O7): no bridge call draws a temporary scene object"
  - "The Properties window keeps its ImGui name 'Properties' (it shares an ID with the docked panel, so its fields append to that panel): renaming it floats it over the map and the M2/M3 scenarios' map clicks then hit it; recorded in deferred-items.md"
  - "The three user rulings of 2026-10-03 are decisions in PARITY (L4, MM1, the final note), not open rows; the script-path and wheel-delta rulings were already done"

metrics:
  duration: "about 11 h of wall clock (long gates, two Windows hangs and the harness modifier bug took most of it)"
  completed: "partial - see status"

commits: 8
plan_head_before: 741cabe9712725f190316b043098373633d69df7
actuals:
  tokens: 45000
  tasks: 3
  commits: 8
---

# Phase 5 Plan 11: App shell, single instance, parity closure and the phase gates - stopped at the deletion decision

**Status: PARTIAL. Tasks 1, 2 and 3 are done and committed; the run STOPPED at Task 4, the one-way decision to delete the MFC editor. Task 5 (the deletion) was not started: `Sources/src/MapEditor` and `Sources/src/bin/MapEditor.exe` are untouched.**

One-liner: Options, View with Reset layout, drag-drop, Help/About and a per-user single-instance socket that works on macOS, Linux and Windows; every one of the 152 PARITY rows closed with evidence (the NF rows re-read against the MFC source and one cited wrongly corrected); three real parity gaps the closure turned up fixed; every local gate green.

## Accomplishments

- **Task 1 (`4bb5667cf`)** - Tools > Options (`game_parameters` into Test in game's argv between the editor's own arguments and the map name, split on spaces with `"..."` groups and nothing else interpreted; the default save format), View with a check per docked panel (`PanelId`), the status bar and every floating window (`ViewWindow`), Reset layout (`bk_imgui_reset_window_layout` over ImGui's `ClearWindowSettings`), `layout.ini` kept beside `mapeditor.cfg` (interactive mode only), SDL drop events through the unsaved-changes guard, Help > Keys and tools (tools from `tool_registry`, keys from `view_math.key_help`) and About; commands `options_show`, `options_set_gameparams`, `options_set_format`, `layout_reset`, `view_panel`, `help_keys`, `about_show`, `drop_file` and predicates `game_parameters`, `default_format`, `panel_visible`, `layout_default`; map-editor-m3-auto frames 1402-1510.
- **Task 2 (`2f7dd1b96`, fixes `6d04800dd`, `9e92edea8`, `d76e3fadb`)** - `single_instance.zig`: endpoint `<user root>/mapeditor/instance.sock` (the engine's own user-root rules, a short per-user name under the temporary folder when too long for a socket address), one line one path, an owner whose accept loop and four connection threads answer `ok` as soon as the line is queued, a main-loop poll once a frame, an empty line that only raises the window, stale files and hung owners replaced after a timeout. Nine unit tests over the real mechanism run on macOS and on Windows.
- **Task 3** - 05-PARITY.md closed and the gates recorded (below); the spec's editor-set table and user-data section updated; 05-VALIDATION.md's D-40 rows; deferred-items.md weighed item by item; this file with the hand-try checklist.
- **Parity gaps found and fixed while closing rows** (`ed883d613`): PARITY O7 (the Place tool's ghost) had no evidence because M1 never drew one; the Heights minimap now paints refused heights red as the MFC did (`isValidHeight`, pinned against the engine's own function in the bridge tier); the direction wheel's hit area is the whole dial; Help answers F1 as the MFC's Contents did.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] The Windows hand-off hung (two separate causes)**
- **Found during:** Task 2, running the socket tests on win-home: the "hung owner" test never ended
- **Issue:** on Windows a connect to a listening AF_UNIX socket whose owner never accepts completes, and the following read of the answer is not woken by a socket shutdown, so a hung owner held the second launch for ever - exactly what T-05-11-02 forbids
- **Fix:** the second launch's whole exchange runs on a worker thread the waiter abandons after the timeout; the owner serves each connection on its own thread (a test covers two silent clients)
- **Files modified:** Sources/editor/app/single_instance.zig
- **Commits:** 6d04800dd, 9e92edea8

**2. [Rule 1 - Bug] CI's Windows engine tier failed the socket tests (AddressFamilyUnsupported)**
- **Found during:** CI run 37094312614 (the unit tier, which builds for the host, passed; the engine tier, which builds for `-Dtarget=x86_64-windows-msvc`, failed 5 tests)
- **Issue:** `std.Io.net` keeps `UnixAddress` behind `builtin.os.version_range.windows.isAtLeast(.win10_rs4)`, unknown (read false) for a target naming no Windows version - so a plain MSVC build, the shipped editor included, would have had no single instance
- **Fix:** build.zig names Windows 10 1803 as the oldest Windows unless the command line pins a version; verified on win-home (engine tier 208 pass, 2 skip, 0 fail)
- **Files modified:** build.zig
- **Commit:** d76e3fadb

**3. [Rule 1 - Bug] The BK_EDITOR_AUTO harness left Ctrl held in ImGui**
- **Found during:** Task 3, a scripted press on the direction wheel late in the M3 scenario did nothing (ImGui's right button was down)
- **Issue:** a scripted key went up with its modifiers still set, so after `key=Z+ctrl` ImGui believed Ctrl stayed down and on macOS took every later left click for a right click
- **Fix:** the key-up carries no modifiers; a failed step now also prints ImGui's pointer, hovered and active widgets and the focus
- **Files modified:** Sources/editor/app/smoke.zig, Sources/editor/imgui/imgui_backend.cpp/.h
- **Commit:** 708f0e75e

**4. [Rule 2 - Missing critical functionality] PARITY O7, the placement ghost, had no evidence and no code**
- **Found during:** Task 3, chasing the empty Evidence cells
- **Fix:** `markers.drawPlacementGhost` (the object's palette picture at 50% over a ring); recorded as a difference from the MFC's engine-sprite ghost
- **Commit:** ed883d613

**5. [Rule 1 - Bug] 05-PARITY.md cited L16 and D3 wrongly and hid evidence in 21 rows**
- **Issue:** L16 said the Storage coverage button is in no toolbar (it is, editor.rc:1961; only its computation is dead); D3 cited `iMissionInternal.cpp:1465` (it is :1471); 21 rows had one cell too many, so their evidence sat in a column GitHub's table renderer drops
- **Fix:** corrected in place, rows normalised
- **Files modified:** 05-PARITY.md

### Differences from the plan's text (recorded, not bugs)

- **The agent-namespace branch guard of the executor protocol (`agent-*`) was not applied:** the user's instruction names the worktree and branch `feat/map-editor-m3` explicitly and no other agent works in it; the protected-branch guard (not `main`) held.
- **Windows is a socket, not a named pipe** (key-decisions above).
- **Two things the plan asked for could not be run as written:** `map-editor-m3-auto` on win-home over ssh (no GUI; **user-waived 2026-10-03**) and a macOS or win-home *interactive* double launch with a visible window - the macOS double launch ran with hidden editors (below).
- **Environment, not committed:** the visible-window M1 smoke at the head of the m3-auto chain needs the real pointer over its window; the pointer was parked there with `cliclick m:700,450` before each run. Without it the smoke fails with `mouse focus false`, the known flake.
- **The extra `fix(build)` and harness commits** are why `commits: 8` where the plan had three task commits (the count is measured at the write of this file and includes the Task 3 docs commit `5c8808f1a`; this file and STATE.md come after).

**Total deviations:** 5 auto-fixed (4 bugs, 1 missing functionality). **Impact:** the single-instance feature is now tested on Windows and shipped there; nothing else changes the plan's contract.

## Verification and gates

All local gates run one at a time on macOS arm64 with `-Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run` (logs in `zig-out/local-test/`, prefix `05-11-`):

| Gate | Result |
|---|---|
| `zig build test` | rc 0 |
| `map-editor-smoke` (visible window, pointer parked) | `map-editor: smoke PASS (52 steps, 260 objects ...)` |
| `map-editor-auto`, `map-editor-auto-m2`, `map-editor-m3-auto` (one chain, `05-11-t3-auto.log`) | done (13), done (298), done (659 actions), rc 0 |
| `test-map-editor-engine` | `map-editor-engine: PASS (260 objects)`; `M3 height rule: 0 of 9409 vertices refused` |
| `test-editor-bridge` | `editor-bridge: PASS` (`M3 height rule ok`) |
| `test-rmg-composer-roundtrip` | `composer-roundtrip: 43 templates, 102 graphs, 404 containers, 27 field sets ok` |
| `test-rmg-determinism` | `byte identical ok` x3 (incl. the authored set), PASS |
| `map-editor-game-reads-it-m3` | `game reads it M3 PASS` |
| `test-random-missions -Drandom-missions-sweep=cover` | `208 cases, 0 failed` (3984 s) |
| `test-map-files` / `test-map-files-all` | `66 of 66` / `1755 of 1755 maps round-tripped`, PASS, 0 FAIL |
| `zig test tools/zig/build_hermeticity_test.zig` (after the build.zig edits) | 3/3 |
| `map-editor-m3-auto map-editor-game-reads-it-m3 --release=fast` | rc 0 in 9 min 36 s on `zig-out/game/macos/arm64/release` (MapEditor 7,477,656 bytes, Game 2,932,648): smoke PASS (52 steps), `map-editor-auto` 13, `map-editor-auto-m2` 298, `map-editor-m3-auto` 659 actions, `game reads it M3 PASS` (`05-11-t3-release.log`) - the scripted walk-through on the release build, so the hand try starts from a build that already passes every scenario |
| win-home (x86_64-windows-msvc, no GUI) | core 374/374; app tiers 352/354 (2 skipped, the nine socket tests included); engine tier (run) 208 pass, 2 skip; MSVC compile of the bridge test, the determinism and round-trip tools; MapEditor build rc 0 |
| macOS double launch (hidden editors, `05-11-dl-check.sh`) | second launch exit 0 in 0.23 s, owner logs `map open from second instance`; raise-only launch exit 0; stale socket after `kill -9` does not block a start; clean exit removes the socket |
| CI | run 37098890101 at d76e3fadb, dispatched 2026-10-03: **all six jobs green** - linux-platform 4m16s, linux-arm-platform 1m18s, macos-intel-platform 2m32s, macos-platform 23m5s, windows-mingw-platform 5m48s, windows-platform 1h46m (its Engine tier, Map editor engine tier, Random missions tier, RMG composer round trip, RMG determinism and the packages all green). The run before it, 37094312614 at ed883d613, failed only the Windows Map editor engine tier's five socket tests (deviation 2), fixed by d76e3fadb. |

## Hand try (D-40.10) - Johannes's backstop, on the release build

Build: macOS `zig build install-map-editor --release=fast -Dtarget=aarch64-macos -Dcopy-data=false`, then run `zig-out/game/macos/arm64/release/MapEditor`; Windows: the same command with `-Dtarget=x86_64-windows-msvc` on a machine with a desktop session, or `zig build package-game-editors --release=fast` and unzip the editors package. Look at each stop and tick it:

1. **Start with no map.** View: every panel and window has a check; hide Tools, Sounds and the status bar, then Reset layout brings them back. Help > Keys and tools (also F1) lists the tools and keys; About names the product and the licence.
2. **New map** (Cmd/Ctrl+N, 16x16 summer): the title says `*`, the size and the mod; the status bar shows VIS and SCRIPT coordinates.
3. **Heights** (tool Heights, key 0 if offered): raise, lower, Alt+drag level; Generate hills; Ctrl+drag a cliff, then watch the Minimap's Heights picture turn those vertices red.
4. **Fields** panel, **Objects**: with the Place tool a half-transparent picture of the chosen object follows the pointer; place a squad; the wheel (drag over the *whole* dial, lower half too) turns the selection by the amount you drag; band-select; drop a unit on a bunker to garrison it; double-click opens Properties.
5. **Players and Unit Creation Info**, **Check Map** (Fix all is one undo step).
6. **Layers**: each toggle; Depth Complexity is greyed (accepted); Unit Fire Ranges.
7. **Minimap**: click moves the camera; Create Minimap Images after a Save As.
8. **Save as XML / BZM**, **Test in game** - and Tools > Options: put `-windowed` or another harmless parameter in Game parameters, OK, Test in game starts the game with it.
9. **Create Random Map** (a template, a seed): the map opens; move the map and its `.lua` to another folder: Test in game runs the script from there.
10. **Composers** (Containers, Graphs, Fields, Templates): open a shipped file, edit, Check!, Save As, regenerate a map from it.
11. **Tools > Export lists** (the four lists land under the user folder).
12. **Drag a `.bzm` from Finder/Explorer onto the window**: it opens (a dirty map asks first); a `.txt` is ignored with a note. **Single instance:** launch `MapEditor <another map>` while one runs: the running window comes forward and opens it, the second process exits.
13. Repeat 1-12 on the Windows release build (the GPU look of roads, rivers, wire frame and the minimap on D3D12/Vulkan is the part nothing else checks).

## Checkpoint: Task 4 - delete the MFC editor (one-way, D-38)

**Decision: `delete-now` - approved by Johannes 2026-10-03, delete-now.** (Relayed by the coordinating agent after the Place-tool ghost, PARITY O7, was closed against the MFC code while it was still in the tree: commit `9d0ebb572`.) Evidence for it:

- **Parity.** 152 rows, none empty. 100 M3 and shared rows closed with named tests and scenario frames; 18 NF rows (L16, T7, E1-E4, TR13, TR19, O22, MM5, R13, R16, R17, D1-D5) closed with file:line citations re-read on 2026-10-03 (one correction); 19 M1 rows closed by phase 3's verification and the tests their cells name; 15 M2 rows by phase 4 (CI 36692345194); the user-waived items (Depth Complexity L4, the minimap marker filter MM1) noted in their rows. The full table is below.
- **Gates.** Every local gate green (table above); CI run 37098890101 green on all six jobs.
- **What stays open.** (a) D-40.10, the hand try; (b) the Windows GUI legs, user-waived; (c) the items under "Open and weighed" below.
- **Recommendation: `delete-now`** (CI run 37098890101 is green on all six jobs). Git history keeps the tree; the only thing `hold` buys is greppability during the hand try, and the deletion task's own grep gate, build and full matrix re-run are ready. If you would rather have the tree beside you for the hand try, `hold` costs nothing but D-40.9 staying open.

### Open and weighed (the checkpoint's "any rows still open")

No PARITY row is open. What the human should know before approving:

1. **O7 (placement ghost) is a picture, not the engine's sprite**, and it does not turn with the wheel. Fixed here, recorded as a difference.
2. **V3: ImGui docking is not switched on.** The docked columns are fixed, the other windows float, View manages all of them.
3. **Not verifiable without a desktop:** a real Finder/Explorer drag onto the window (the scripted drop uses the command path), a Windows double launch of the real editor (the socket tests pass on Windows over the real mechanism), the Windows GPU look.
4. **Found, not fixed:** the Properties window shares its ImGui ID with the docked Properties panel (deferred-items.md "From 05-11").
5. **Real parity gaps left open for your call** (each weighed in deferred-items.md, none needed by a PARITY row): no thumbnails in the Fields Composer's tabs (needs a bridge call that renders a tile of a named tileset); the Settings "Maps folder" does not move a generated map (the MFC always generated under the Data root); Browse takes one file where the MFC's Add took several; a party change does not rename existing flags and a shared link ID is reported not fixed (both the preservation rule against the MFC's silent behaviour).

### Parity closure table

All 152 rows of `05-PARITY.md` (the evidence cells there are full text):

| Row | Owner | Status | Evidence (short; the full text is in 05-PARITY.md) |
|---|---|---|---|
| F1 | M3 05-01 | closed | `editor-bridge: M3 new map ok` (TestM3NewMap); `map_new:8x8:summer:M3Auto` in map-editor-m3-auto; panels_logic "new... |
| F2 | M1 | closed (M1, phase 3) | 03-VERIFICATION |
| F3 | M1 | closed | test-map-files |
| F4 | M1 / M3 05-05 | closed | `editor-bridge: M3 check map ok` (an object whose type no database lists is deleted by Fix all and its undo saves the... |
| F5 | M3 05-01 | closed | TestM3NewMap's altitude-less crafted-map case (`editor-bridge: M3 new map ok`) |
| F6 | M1 | closed (M1, phase 3) | 03-VERIFICATION |
| F7 | M1 | closed (M1, phase 3) | 03-VERIFICATION |
| F8 | M3 05-01 | closed | TestM3NewMap saves both formats and reads them back equal; `file_save_xml`/`file_save_bzm` commands;... |
| F9 | M1 | closed (M1, phase 3) | 03-VERIFICATION |
| F10 | M3 05-08 | closed | `editor-bridge: M3 create random map ok` (TestM3CreateRandomMap: every refusal names its field and writes nothing, 19... |
| F11 | M1 | closed (M1, phase 3) | 03-VERIFICATION |
| F12 | M3 05-11 | closed | an SDL drop event (`SDL_EVENT_DROP_FILE`, main.zig `run`) goes to `panels.dropFile`, which checks the path... |
| F13 | M3 05-11 | closed | `single_instance.zig` - a per-user stream socket (a Unix domain socket on macOS and Linux; on Windows AF_UNIX through... |
| F14 | M3 05-11 | closed | a map path on the command line opens at startup: `main.zig interactive` -> `mapArgument` (made absolute against the... |
| F15 | M3 05-01 | closed | panels_logic formatTitleM3 tests; `expect=title:` steps in map-editor-m3-auto (coldwinter, M3Auto, m3.bzm, 8x8) |
| M1 | M3 05-02 | closed | `map-file: M3 fill ok` (TestM3FillRegion: fill undone byte-identical, crosses by content); `editor-bridge: M3 update... |
| M2 | M2 | closed (M2) | `map-file: M2 camera anchor records ok`, `editor-bridge: M2 camera anchors ok`, `map-editor-engine: M2 camera anchors... |
| M3 | M1 | closed (M1, phase 3) | 03-VERIFICATION |
| M4 | M3 05-05 | closed | `map-file: M3 players ok` (InsertPlayer/ErasePlayer and their inverse write the unedited file byte for byte, the... |
| M5 | M3 05-05 | closed | `map-file: M3 players ok` (a put, the put that grows the vector and their inverses are byte-exact); `editor-bridge:... |
| M6 | M2 | closed (M2) | `editor-bridge: M2 script file ok`; `map-editor-auto-m2` names the script, chooses it beside a user map... |
| M7 | M3 05-05 | closed | core `checks.zig` tests (every kind on a table of fixtures: duplicates, invalid/duplicate links, player index, party,... |
| M8 | M3 05-02 | closed | `editor-bridge: M3 update and fill ok` (TestM3UpdateMapAndFill: the composite's progress heard 7+snapped, the snap... |
| M9 | M3 05-02 | closed | `do=instant_update` + `expect=undo_depth:5` in map-editor-m3-auto (a setting, never an undo step); the session's flag... |
| M10 | M3 05-02 | closed | `do=fit_grid` + `expect=undo_depth:5` in map-editor-m3-auto; `editor-bridge: M3 update and fill ok` (the update pass... |
| M11 | M3 05-07 | closed | as MM4 - Map > Create Minimap Images and the panel's Create button run `minimap_create`; a shipped or never-saved map... |
| U1 | M2 | closed (M2) | `editor-bridge: M2 start commands ok`, `map-editor-engine: M2 start command round trip ok`, `map-editor-auto-m2`... |
| U2 | M2 | closed (M2) | `map-editor-auto-m2` `startcmds_window:1` and shot `m2_startcmds_panel`; commands `startcmd_select`,... |
| U3 | M2 | closed (M2) | `editor-bridge: M2 reserve positions ok`, `map-editor-auto-m2` frames 273–303; the game applies it (`reserve applied... |
| L1 | M3 05-06 | closed | probe (arnheim, 640x480): `Terrain` call ok, reads back yes, scene flag agrees, frame changed 154401 px (middle) /... |
| L2 | M3 05-06 | closed | probe: `Grid` 9759 px (middle) / 5204 px (corner), put back 0 / 0; m3-auto `differ` shot `m3-layers-grid` 0.18%. The... |
| L3 | M3 05-06 | closed | Measured (A4): GFXGPU dropped `GFXGPU_STATE_WIREFRAME` in its state switch, so `IGFX::SetWireframe` changed 0 px; the... |
| L4 | M3 05-06 | closed (user-waived item noted) | Measured (A4), not drivable on the GPU renderer: the probe, driving the scene's flag directly because the bridge's... |
| L5 | M3 05-06 | closed | probe: `Terrain Noise` 6287 px (middle) / 15497 px (corner), put back 0 / 0. Noise is a terrain-owned flag like the... |
| L6 | M3 05-06 | closed | probe: `Black Stripes` 0 px (middle) / 599 px (corner), put back 0 / 0 - the border is drawn outside the map's edge,... |
| L7 | M3 05-06 | closed | probe: `Units` 1933 px (middle) / 0 px (corner), put back 0 / 0. `editor-bridge: M3 layers ok` (TestM3Layers: the... |
| L8 | M3 05-06 | closed | probe: `Objects` 154786 px (middle) / 94502 px (corner), put back 0 / 0. `editor-bridge: M3 layers ok` (TestM3Layers:... |
| L9 | M3 05-06 | closed | probe: `Bounding Boxes` 699 px (middle) / 0 px (corner), put back 0 / 0; the re-apply keeps it across an open... |
| L10 | M3 05-06 | closed | probe: `Shadows` 16326 px (middle) / 9596 px (corner), put back 0 / 0. `editor-bridge: M3 layers ok` (TestM3Layers:... |
| L11 | M3 05-06 | closed | probe: `Haze` (the MFC's "Hase" spelled right) 999 px (middle) / 1457 px (corner), put back 0 / 0; on by default as... |
| L12 | M3 05-06 | closed | probe: `War Fog` 254246 px (middle) / 144219 px (corner), put back 0 / 0; m3-auto `differ` shot `m3-layers-war-fog`... |
| L13 | M3 05-04 | closed | core `selection circles: the inner ring rides the footprint, the outer follows, a small object never vanishes`... |
| L14 | M3 05-06 | closed | probe: `Units Passability` 9077 px (middle) / 6113 px (corner), put back 0 / 0 - the green marks round the buildings... |
| L15 | M3 05-06 | closed | `editor-bridge: M3 layers ok` (TestM3Layers: the selected mode registers the selection's firing units - skipping link... |
| L16 | NF | closed (NF, cited) | the MFC does have the button - `ID_SHOW_STORAGE_COVERAGE` is in the Tools toolbar `IDT_TOOLS_BUTTONS`... |
| V1 | M3 05-11 | closed | View lists a check item for every docked panel and the status bar (`panels_logic.PanelId`: Tools palette, Objects,... |
| V2 | M3 05-11 / 05-07 | closed | the Minimap bar: View > Minimap toggles the panel (`minimap_toggle`, `expect=minimap_visible:1` in... |
| V3 | M3 05-11 | closed | ImGui windows replace the MFC's customisable toolbars: every panel and window can be moved and resized, hidden by... |
| V4 | M3 05-01 | closed | core test "the brush takes sizes 1..16, even sizes hanging right and below"; `brush_size` command; palette combo... |
| V5 | M1 | closed (M1, phase 3) | 03-VERIFICATION |
| V6 | M3 05-01 | closed | panels_logic visScriptLine/objectLine tests; `expect=status:VIS:` / `expect=status:SCRIPT` steps in... |
| V7 | M1 | closed | view tests |
| H1 | M3 05-11 | closed | Help > Keys and tools (F1 too): the Tools table is built from `tool_registry.entries` (label, digit key and the... |
| H2 | M3 05-11 | closed | Help > About: the product line, the milestone, the design document's path, the source and the licence line... |
| T1 | M1 | closed | map-editor-auto |
| T2 | M3 05-11 / 05-01 | closed | Tools > Options holds the game's extra command line and the default save format, committed on OK as one settings... |
| T3 | M3 05-08 | closed | panels_logic `export lists: the graphs list is each template then its graphs as index, weight and name` (the MFC's... |
| T4 | M3 05-08 | closed | panels_logic `export lists: names one per line with CRLF` and `each kind has the MFC's file, folder and extensions`... |
| T5 | M3 05-08 | closed | panels_logic export tests (the `.bzm` files, then the `.xml` ones, as the MFC's two parameters enumerate); bridge... |
| T6 | M3 05-08 | closed | panels_logic `export lists: names one per line with CRLF, the Maps list without the two 3D maps` (`maps\river3d.xml`,... |
| T7 | NF | closed (NF, cited) | MainFrm.cpp:1138 re-read, and editor.rc:2132-2135 lists Tools 0-3 only (no menu item for ID_TOOL_4): `OnTool4` loads... |
| T8 | M1 | closed (M1, phase 3) | 03-VERIFICATION |
| E1 | NF (exceeded) | closed (NF, cited) | `OnEditUndo` (TEF:2659) has its whole body inside `/ ... //` (TEF:2661-2669; the undo stack is never pushed to), so... |
| E2 | NF | closed (NF, cited) | no `ON_COMMAND(ID_EDIT_CUT, ...)` exists anywhere under `Sources/src/MapEditor` (the id appears only as the toolbar... |
| E3 | NF | closed (NF, cited) | `OnEditCopy` (TEF:2237) has its body inside `/ ... //` (TEF:2239-2257), so nothing is copied. |
| E4 | NF | closed (NF, cited) | `OnObjectPaste` (TEF:2230) forwards to `CObjectPlacerState::OnObjectPaste` (ObjectPlacerState.cpp:1092), whose whole... |
| TR1 | M1 | closed (M1, phase 3) | 03-15 |
| TR2 | M3 05-02 | closed | `editor-bridge: M3 tile info ok` (TestM3TileInfo: tile 0 described - name, variants, index; every offered tile... |
| TR3 | M1 (+V4) | closed (M1, phase 3) | 03-VERIFICATION |
| TR4 | M3 05-02 | closed | `editor-bridge: M3 heights ok` (TestM3Heights: the editor\\profile.tga pattern, brush 2..16, BAD_ARGUMENT outside);... |
| TR5 | M3 05-02 | closed | `editor-bridge: M3 heights ok` (raise, lower, level strokes, each vs the same-function expected map and undone... |
| TR6 | M3 05-02 | closed | `editor-bridge: M3 heights ok` (all four level modes - zero, click tile, instant average, click average - each... |
| TR7 | M3 05-02 | closed | `editor-bridge: M3 heights ok` (speed and ratio ride the pattern math the expected maps build); the Heights panel's... |
| TR8 | M3 05-02 | closed | `editor-bridge: M3 heights ok` (the cliff-making stroke REFUSED with the MFC's own words, nothing changed; Ctrl keeps... |
| TR9 | M3 05-02 | closed | the heights brush draws the M1 brush's own footprint outline at the cursor (view.zig, the heights tool's corner... |
| TR10 | M3 05-02 | closed | `editor-bridge: M3 heights ok` (Hills/Rocks/Dunes generate with the MFC formula's own bounds and both range ends... |
| TR11 | M3 05-02 | closed | the MFC's Update button is Update Map - one command, one undo step (`do=map_update` in map-editor-m3-auto,... |
| TR12 | M3 05-02 | closed | `editor-bridge: M3 heights ok` (set zero vs the same-shades expected map, undone byte-exact); the Heights panel's Set... |
| TR13 | NF | closed (NF, cited) | the radios `IDC_SHADE_TYPE1` (Multi) and `IDC_SHADE_TYPE2` (Heterogenous) are `NOT WS_VISIBLE / WS_DISABLED` in... |
| TR14 | M3 05-03 | closed | BkEditorListRmg walks the mounted storages' Scenarios/FieldSets (storage-relative, lowercased, .xml stripped; a... |
| TR15 | M3 05-03 | closed | RandomizeEdges with the MFC's exact arguments (cut, 10, width, disturbance point, min*cell, 512*cell, true), the... |
| TR16 | M3 05-03 | closed | ApplyFieldInSession runs the passes over one polygon (ValidateFieldSet + FillTileSet; FillObjectSet into a scratch... |
| TR17 | M3 05-03 | closed | the mismatch asks before applying, the MFC's dialog above PlaceField: `fields_apply` refuses naming both seasons and... |
| TR18 | M3 05-03 | closed | tools_fields.zig's three states with the MFC keys - click adds, right-click takes the rubber and the last,... |
| TR19 | NF | closed (NF, cited) | `IDC_FIELD_REMOVE_OBJECTS_CHECK_BOX` is `NOT WS_VISIBLE / WS_DISABLED` (editor.rc:1333-1335) and no `.cpp` names it... |
| O1 | M1 (+M3 05-04 for the  | closed (M1, phase 3) | the object palette lists the catalogue by game type with each object's own icon (`BkEditorObjectPicture`;... |
| O2 | M3 05-03 | closed | the nine quick toggles (`filter_toggle:<slot>`), Ctrl+click's assign (`filter_assign`, persisted as filter_setup in... |
| O3 | M3 05-03 | closed | the combo (`filter_select`, `none` clears) and New/Delete Filter (`filter_new`/`filter_delete`) over the merged list... |
| O4 | M3 05-03 | closed | drawFiltersComposer (the Tools window): conditions of folder words with Add/Delete/Rename... |
| O5 | M1 | closed (M1, phase 3) | the placement player is `View.placer.player` (the palette's Player combo and the toolbar's V5); a placed object takes... |
| O6 | M3 05-04 | closed | By the delta - the user's decision of 2026-10-03, which overrides the MFC's "set to angle" (TEF:1053-1090 turned... |
| O7 | M1 | closed | while the Place tool is active the object about to be placed follows the pointer at half opacity, the MFC's... |
| O8 | M1 | closed (M1, phase 3) | a click places the palette's object (`BkEditorAddObject` through `Editor.addObject`, one undo step); a flag takes its... |
| O9 | M1 (single) / M3 05-04 | closed | core `a click on a squad member selects the whole squad`; the picks answer a soldier's squad link... |
| O10 | M3 05-04 | closed | core `Ctrl+click toggles members, a plain click replaces, and a press on nothing clears the whole set` (the scripted... |
| O11 | M3 05-04 | closed | core `the band on empty ground selects by screen rectangle`, `the Ctrl band selects by tile rectangle, passing the... |
| O12 | M1 (single) / M3 05-04 | closed | core `a group drag moves the whole selection as one undo step, and undo restores every member`; `editor-bridge: M3... |
| O13 | M3 05-04 | closed | `editor-bridge: M3 properties and links ok` (TestM3PropertiesAndLinks: a live garrison pair through... |
| O14 | M3 05-04 | closed | core `the selector's right click alone deselects, and a right click with the left held cycles`; map-editor-m3-auto... |
| O15 | M3 05-04 | closed | view `in Select a double click, Enter or Space on a selection asks for the Properties window (O15)` (the Select tool... |
| O16 | M1 (single) / M3 05-04 | closed | multi (05-04): core `Delete takes the whole selection through the cascade as one undo step`; `editor-bridge: M3... |
| O17 | M3 05-04 | closed | panels_logic `property kinds: the catalogue's game types answer their MFC manipulator's set` (building: units, Script... |
| O18 | M3 05-04 (fields) / M2 | closed | panels_logic `property kinds: the catalogue's game types answer their MFC manipulator's set` (trench: units, Script... |
| O19 | M3 05-04 | closed | panels_logic `property kinds ...` (unit: units, angle, Script ID, scenario unit, player, health, formation) and... |
| O20 | M3 05-04 | closed | drawMultiFields (angle, Script ID, player; the MFC's Behaviour combo is commented-out code there and is not offered)... |
| O21 | M3 05-04 | closed | the units list's double-click unlink (panels_m3 drawUnitsList -> `link_unlink`); core `the drop links, garrisons... |
| O22 | NF | closed (NF, cited) | the in-tab button `IDC_SO_DIPLOMACY_BUTTON` is a 6x6 `NOT WS_VISIBLE / WS_DISABLED` control (editor.rc:371-372) and... |
| VO1 | M2 | closed (M2) | `map-file: M2 fence plan ok`, `editor-bridge: M2 fences ok`, `map-editor-auto-m2` frames 97–118; the fence run... |
| VO2 | M2 | closed (M2) | `map-file: M2 bridge plan ok`, `editor-bridge: M2 bridges draw ok`, `map-editor-engine: M2 bridge round trip ok (5... |
| VO3 | M2 | closed (M2) | `editor-bridge: M2 bridge rotate and toggle ok`, `editor-bridge: M2 bridge delete ok`; `map-editor-auto-m2` Enter,... |
| VO4 | M2 | closed (M2) | as O18 drawing, plus `map-file: M2 trench overlay ok` and `map-editor-auto-m2` frames 119–139; the trench appears in... |
| VO5 | M2 | closed (M2) | the Roads & Rivers panel; in mode All the width and opacity sliders re-width the selected line as one undo step (core... |
| VO6 | M2 | closed (M2) | `map-file: M2 vso builder ok`, `editor-bridge: M2 roads ok`, `editor-bridge: M2 road edits ok (8 edits)`,... |
| VO7 | M2 | closed (M2) | `editor-bridge: M2 rivers and passability ok` (present after add, gone after delete); the game reads one more river... |
| MT1 | M3 05-04 | closed | the Damage tool (tools_damage.zig; the Map Tools panel's whole-percent field, default 10, panels_m3 drawDamageTool):... |
| MT2 | M2 | closed (M2) | `map-file: M2 script areas ok`, `editor-bridge: M2 script areas ok`, `map-editor-auto-m2` frames 186–232; the game's... |
| G1 | M2 | closed (M2) | `editor-bridge: M2 groups ok`, `editor-bridge: M2 hide checked ok`, `map-editor-engine: M2 groups round trip ok`,... |
| AI1 | M2 | closed (M2) | `map-file: M2 parcel formulas ok`, `editor-bridge: M2 ai general ok`, `editor-bridge: M2 ai general edits ok`,... |
| MM1 | M3 05-07 | closed (user-waived item noted) | `panels_logic` tests (the 17 colours value for value against MiniMapTypes.cpp:149-167, terrain pixels, the height... |
| MM2 | M3 05-07 | closed | `panels_logic` tests of the MFC click formula (rect edges, a non-square map) and of the screen-centre offset rule;... |
| MM3 | M3 05-07 | closed | `editor-bridge: M3 minimap images ok` (`BkEditorMinimapImage` finds `<map>.tga` first, then `<map>_h.dds` -... |
| MM4 | M3 05-07 | closed | `editor-bridge: M3 minimap images ok` (one CreateMiniMapImage call with the MFC's four parameters writes... |
| MM5 | NF | closed (NF, cited) | `IDC_MINIMAP_CLOSE` is `NOT WS_VISIBLE` (a 6x6 control at the dialog's corner, editor.rc:394-395; its handler... |
| R1 | M3 05-09 | closed | Tools > Containers Composer (panels_m3 `drawContainersComposer`): the MFC list's twelve columns (Path, Size, Count,... |
| R2 | M3 05-09 | closed | add patches from the storages (the picker lists every patch map under Scenarios\\Patches, multi-select;... |
| R3 | M3 05-09 | closed | the Properties popup (Space, double-click or the button): the setting (a combo over the Scenarios\\Settings scan or... |
| R4 | M3 05-09 | closed | Tools > Graphs Composer (`drawGraphsComposer`): the list's eleven columns for the open graph (Path, Max Size, Nodes,... |
| R5 | M3 05-09 | closed | the node properties popup (double-click or right-click > Properties on a node away from its links): the node's... |
| R6 | M3 05-09 | closed | the link properties popup (double-click or right-click > Properties on a link; several links under the point are... |
| R7 | M3 05-10 | closed | `editor-bridge: M3 rmg field sets ok` (TestM3RmgFieldSets, real engine); `composer-roundtrip: 43 templates, 102... |
| R8 | M3 05-10 | closed | `editor-bridge: M3 rmg field sets ok` (TestM3RmgFieldSets, real engine); `composer-roundtrip: 43 templates, 102... |
| R9 | M3 05-10 | closed | `editor-bridge: M3 rmg field sets ok` (TestM3RmgFieldSets, real engine); `composer-roundtrip: 43 templates, 102... |
| R10 | M3 05-10 | closed | `editor-bridge: M3 rmg field sets ok` (TestM3RmgFieldSets, real engine); `composer-roundtrip: 43 templates, 102... |
| R11 | M3 05-10 | closed | `editor-bridge: M3 rmg templates ok` (TestM3RmgTemplates, real engine); `composer-roundtrip: 43 templates, 102... |
| R12 | M3 05-10 | closed | `editor-bridge: M3 rmg templates ok`; the round-trip line above with its QuickLoadMapInfo check; `rmgt_popup` shots... |
| R13 | NF (added anyway) | closed (NF, cited) | `editor-bridge: M3 rmg templates ok` (Check! on every shipped template: no errors in the template's own rules,... |
| R14 | M3 (replaced) | closed | the composers list their files by scanning the storages' folders (`BkEditorListRmg` kinds 2 and 3,... |
| R15 | M3 05-11 | closed | ImGui's own ini: `layout.ini` beside mapeditor.cfg (the user root's `mapeditor` folder, or beside the... |
| R16 | NF | closed (NF, cited) | `IDD_TAB_RANDOM_MAP_GENERATOR` is defined at editor.rc:402 and numbered at resource.h:122, and no `.cpp` or `.h`... |
| R17 | NF | closed (NF, cited) | `PESelectStringsDialog.h` is included once (UnitCreation.cpp:7) and `CPESelectStringsDialog` is never instantiated... |
| D1 | NF | closed (NF, cited) | No class: `CTabSoundsDialog` is named only inside block comments (TEF:2034-2041, 3217-3236, 4755-4773) and... |
| D2 | NF | closed (NF, cited) | No class or state: `IDD_TAB_VO_FORESTS` (editor.rc:546, resource.h:131) and its `IDC_FORESTS_*` ids are named by no... |
| D3 | NF | closed (NF, cited) | Save-side `AddSounds` is commented out (TEF:3217-3236). The load-side call (TEF:2050) does run, but it only fills... |
| D4 | NF | closed (NF, cited) | Handlers exist (`ON_COMMAND` TEF:450, 453, 457, 461, 464) with no UI: `OnButtonFog` TEF:2356, `OnToolsClearCash`... |
| D5 | NF | closed (NF, cited) | The Groups dialog's `ON_WM_TIMER` (GroupManagerDialog.cpp:56) has no `OnTimer` of its own and no `SetTimer`;... |
| S1 | M3 05-05 | closed | map-editor-m3-auto 424-436: `file_save_bzm` of the defective crafted map saves it, `expect=status:checks` (the status... |
| S2 | M3 05-01 | closed | `map-file: M3 altitude region ok` + `editor-bridge: M3 altitudes ok`: the region primitive shades at edit time... |
| S3 | M1 | closed | test-map-files |
| S4 | M1 / M2 | closed | `editor-bridge: M2 bridges draw ok` (the save equals the `NMapGeometry::PlanBridge` map); `test-map-files-all` 1,755... |
| S5 | M1 (not copied) | closed | test-map-files-all |
| S6 | M3 05-04 / M1 | closed | markers.zig drawScenarioTint rings every scenarioObjects record blue (the records' `scenario` flag) and the... |
| S7 | M3 05-04 | closed | BkEditorSetLink places a garrison beside its host (the MFC's -30,+30 load-time layout) and markers.zig drawLinkLines... |

## Threat Flags

| Flag | File | Description |
|------|------|-------------|
| threat_flag: local-ipc | Sources/editor/app/single_instance.zig | a new per-user socket the running editor reads one line from; covered by the plan's threat model (T-05-11-01/02): per-user path, the line is validated like a drop, no commands, a flood stops at the queue, stale and hung peers time out |

## Known Stubs

None.

## Pending

- The decision of Task 4 (`delete-now` or `hold`), then Task 5 if approved.
- D-40.10, Johannes's hand try (checklist above).
- Phase-level: the code review, the verifier and `phase.complete` after the decision.

## Self-Check: PASSED

Checked after writing: `Sources/editor/app/single_instance.zig`, this file, `05-PARITY.md`, the gate logs (`zig-out/local-test/05-11-*`) and the double-launch script exist; the commits `4bb5667cf`, `2f7dd1b96`, `6d04800dd`, `9e92edea8`, `708f0e75e`, `ed883d613`, `d76e3fadb` and `5c8808f1a` are in the log; `Sources/src/MapEditor` and `Sources/src/bin/MapEditor.exe` are untouched (Task 5 not started).

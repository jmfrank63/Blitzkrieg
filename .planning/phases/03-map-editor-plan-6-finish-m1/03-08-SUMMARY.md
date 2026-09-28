---
phase: 03-map-editor-plan-6-finish-m1
plan: 08
subsystem: map-editor
tags: [zig, cpp, editor-bridge, mods, imgui]

# Dependency graph
requires:
  - phase: 03-map-editor-plan-6-finish-m1 (plan 04)
    provides: "isShippedMap, needsSaveAs, defaultMapsFolder (D-17, D-18, D-28) and formatTitle's read_only suffix, and Pending.switch_mod/NameText left unused for this plan to wire"
  - phase: 03-map-editor-plan-6-finish-m1 (plan 07)
    provides: "state.mod_folder threaded into defaultMapsFolder/showDialog and testlaunch.start, previously always null"
provides:
  - "C ABI: BkEditorMod, BkEditorMods, BkEditorSetMod, BkEditorActiveMod - mirrors CICChangeMOD::Exec without a main loop, never IUserProfile::SetMOD"
  - "session.h/session.cpp: CloseSessionMap - the map-closing half of OpenMapIntoSession, extracted for reuse by BkEditorSetMod"
  - "BkEditorSaveMap stamps szMODName/szMODVersion from the active mod (D-28), only when they differ, untouched with no mod active"
  - "c_bridge.zig: RealBridge.mods/setMod/activeMod"
  - "main.zig: -mod=<Folder>|-mod=None parsed before positional arguments in every mode, applied right after the host starts"
  - "panels.zig: File > Mod submenu, State.reloadCatalogue, State.modFolder()/setModFolder (an owned buffer), the mod's name in the window title"
  - "panels_logic.zig: FileActions.requestSwitchMod/Step.switch_mod, guarded through the existing UnsavedPrompt"
  - "Tracked fixture tools/zig/fixtures/editor_mod/EditorTestMod/data/mod.xml, staged at <install>/mods/EditorTestMod for test-editor-bridge and map-editor-host-check only"
  - "Engine-tier tests TestModsListSetAndClear, TestSaveRecordsTheMod; a third host-check run with -mod=EditorTestMod"
affects: [03-09-unknown-objects-warning-and-object-icons, any later M2/M3 phase touching mods or the object palette]

# Actuals (#2632)
actuals:
  tokens: 17746
  tasks: 3
  commits: 3
plan_head_before: 01a139f530e7fd13f90ae341ad9f6bec4a452a03

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "A mod switch is validated (mod.xml read, name checked) BEFORE anything changes - CloseSessionMap, storage swap, globals - so a refusal (bad argument or unknown folder) leaves the session's mod, its open map and the object database exactly as they were; only a validated switch closes the map first."
    - "The bridge's two-call sizing convention (BkEditorCatalogue, BkEditorObjects) extends to BkEditorMods: a capacity-0 sizing call is REFUSED whenever there is at least one mod - callers read only *out_count from it, never its status."
    - "State.mod_folder became an owned fixed buffer (mod_folder_buffer/mod_folder_len, with modFolder()/setModFolder accessors) instead of a borrowed ?[]const u8 slice, the same reason State.tile_count is a count rather than a slice into tile_buffer: State is returned by value from init, and a slice built during init would point into the copy init leaves behind once it returns."
    - "A mod's display name in the window title is an app-layer post-process appended after panels_logic.formatTitle's own output, not a new formatTitle parameter - keeps that function's own base-game-only test suite unchanged for an app-layer decoration."

key-files:
  created:
    - tools/zig/fixtures/editor_mod/EditorTestMod/data/mod.xml
  modified:
    - Sources/src/EditorBridge/bridge.h
    - Sources/src/EditorBridge/bridge.cpp
    - Sources/src/EditorBridge/session.h
    - Sources/src/EditorBridge/session.cpp
    - Sources/editor/app/c_bridge.zig
    - Sources/editor/app/main.zig
    - Sources/editor/app/panels.zig
    - Sources/editor/app/panels_logic.zig
    - build.zig
    - tools/zig/editor_bridge_test.cpp
    - .gitignore

key-decisions:
  - "BkEditorMods/BkEditorSetMod/BkEditorActiveMod live in bridge.cpp (not session.h/session.cpp): the active-mod fields (szModFolder/szModName/szModVersion) are BkEditorSession's own, the way the plan specified, and the mod-switch logic that touches them - storage swap, globals, the shared managers other than IGFX, LoadDB - stays beside them. Only the map-closing half (shared, engine-facing, already had a model in OpenMapIntoSession) moved into session.h/session.cpp as CloseSessionMap."
  - "The app tracks whether -mod= was given at all (mod_requested), separate from the resolved folder (mod_folder, null for both \"not given\" and \"-mod=None\"): the bridge's own null/\"\" convention treats those two the same, but a plain launch with no -mod= must never pay a mod-switch's FilesInspector/managers/LoadDB cost for nothing."
  - "File > Mod's folder is guarded through the exact same UnsavedPrompt Open and Open Recent already use (Pending.switch_mod, left unused by 03-04/03-07 for this purpose) - one prompt, one modal, for every action that might discard edits."
  - "A refused BkEditorSetMod validates before touching anything: reads and checks mod.xml first, and only calls CloseSessionMap once the mod is known good - so a bad or unknown folder never closes the map it was about to switch away from."

patterns-established:
  - "An owned fixed-buffer field with accessor methods (modFolder/setModFolder), not a borrowed slice, for any State field set after State.init returns by value - the tile_buffer/tile_count precedent, now with a second instance and a named pattern for 03-09+ to follow if another such field is needed."

requirements-completed: [D-09, D-26, D-28, D-23]

# Coverage metadata (#1602)
coverage:
  - id: D1
    description: "MapEditor -mod=<Folder> and -mod=None load a mod's data like the game: mods/<Folder>/data mounted over Data, mod.xml's name/version read, the object database reloaded (D-26)"
    requirement: "D-26"
    verification:
      - kind: integration
        ref: "zig build map-editor-host-check -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run - the third run passes -mod=EditorTestMod and prints \"map-editor: mod EditorTestMod (Editor Test Mod 1.0)\", then host check and panel smoke both PASS"
        status: pass
      - kind: integration
        ref: "tools/zig/editor_bridge_test.cpp TestModsListSetAndClear (zig build test-editor-bridge) - BkEditorMods lists EditorTestMod with its real name/version, BkEditorSetMod mounts it and reloads the catalogue"
        status: pass
    human_judgment: false
  - id: D2
    description: "File > Mod lists None and every installed mod; switching asks about unsaved changes, reloads the object palette and reopens the current map under the new mod (D-26, D-23)"
    requirement: "D-26, D-23"
    verification:
      - kind: unit
        ref: "Sources/editor/app/panels_logic.zig (zig build test-map-editor-panels) - \"file actions: switching mods guards through the unsaved prompt, and an empty folder is None\": a clean map switches at once, a dirty map asks, Cancel keeps the mod, Don't save proceeds"
        status: pass
      - kind: automated_ui
        ref: "zig build map-editor-host-check map-editor-smoke - both PASS with the Mod submenu, performModSwitch and reloadCatalogue compiled into panels.zig's draw path"
        status: pass
    human_judgment: true
    rationale: "The task's own <human-check> asks Johannes (with his own mods installed in the worktree stage's mods folder, never committed) to drive File > Mod by hand: the submenu lists them, switching with unsaved changes asks first, and the palette gains the mod's objects - none of that on-screen behavior is observable from the automated coverage above."
  - id: D3
    description: "Test in game passes the editor's mod as -mod=<Folder> or -mod=None (D-09)"
    requirement: "D-09"
    verification:
      - kind: unit
        ref: "Sources/editor/app/testlaunch.zig (zig build test-map-editor-testlaunch, unchanged by this plan) - buildArgv's own -mod= tests; startTestGame (panels.zig) already passed state.mod_folder before this plan, now a real value once File > Mod sets it"
        status: pass
    human_judgment: true
    rationale: "The task's own <human-check> asks Johannes to confirm F5 starts the game with the mod he selected from the menu - a real game process launched by a live click, not something the automated tiers start."
  - id: D4
    description: "With a mod active, Open and Save As default to <UserRoot>/mods/<Folder>/maps - never the installed, read-only <install>/mods/<Folder>/data - and a saved map records the mod's name and version in szMODName/szMODVersion; with no mod the fields are written as read (D-28, preservation invariant)"
    requirement: "D-28"
    verification:
      - kind: integration
        ref: "tools/zig/editor_bridge_test.cpp TestSaveRecordsTheMod (zig build test-editor-bridge) - saves coldwinter with EditorTestMod active and confirms szMODName/szMODVersion read back as \"Editor Test Mod\"/\"1.0\"; clears the mod, saves again, and confirms both fields equal the shipped file's own"
        status: pass
      - kind: unit
        ref: "Sources/editor/app/panels_logic.zig defaultMapsFolder tests (03-04, unchanged) - mods/<name>/maps with a mod, plain maps without; state.modFolder() now supplies a real folder once File > Mod is used"
        status: pass
    human_judgment: false

# Metrics
duration: ~60min
completed: 2026-09-28
status: complete
---

# Phase 3 Plan 08: Mod loading, File > Mod, and mod-stamped saves Summary

**MapEditor now mounts a mod's data like the game (`-mod=<Folder>`/`-mod=None`, File > Mod), reloads the object palette and Test in game's launch, and stamps a saved map's `szMODName`/`szMODVersion` from the active mod while leaving them untouched with none active.**

## Performance

- **Duration:** ~60 min (approximate: extensive upfront codebase reading before the first commit; the three task commits themselves span ~21 min)
- **Started:** approx. 2026-09-28T07:20:00Z (session start)
- **Completed:** 2026-09-28T08:04:00Z
- **Tasks:** 3 completed
- **Files modified:** 11 (1 created, 10 modified)

## Accomplishments

- New C ABI: `BkEditorMod` (folder/name/version), `BkEditorMods` (every installed mod, sorted by folder), `BkEditorSetMod` (mounts a mod's data, mirroring `CICChangeMOD::Exec` without a main loop: closes the open map first, swaps the `MOD` storage, re-inspects it with `IFilesInspector`, clears the shared managers other than `IGFX` the way `CMainLoop::ClearResources(true)` does, reloads `IObjectsDB`), and `BkEditorActiveMod`. Validates before touching anything, so a bad or unknown folder leaves the session's mod, its open map and the object database exactly as they were. Never calls `IUserProfile::SetMOD` - the editor has no game profile of its own.
- `session.h`/`session.cpp` gained `CloseSessionMap`, the map-closing half of `OpenMapIntoSession` extracted for `BkEditorSetMod` to reuse (world Clear, AI editor Clear, every per-map table reset, `bMapOpen` false).
- `BkEditorSaveMap` stamps `szMODName`/`szMODVersion` from the active mod when they differ (D-28), and leaves both fields exactly as read with no mod active - the preservation invariant every other untouched field of the map keeps.
- `c_bridge.zig`: `RealBridge.mods`/`setMod`/`activeMod`, thin wrappers over the new C calls in the same style `setCamera`/`catalogue` already use.
- `main.zig`: `-mod=<Folder>`/`-mod=None` is pulled out of the argument list before the positional arguments, in every mode (interactive, `--check`, `--smoke`, `--game-reads-it`), and applied right after the host starts - `mod_requested` kept apart from the resolved `mod_folder` so a plain launch with no `-mod=` never pays a mod-switch's cost. `--check` prints `map-editor: mod <folder> (<name> <version>)` once the mod is loaded.
- `panels.zig`: a File > "Mod" submenu (None and every installed mod, `BkEditorMods`, the active one checked); `performModSwitch` (`real.setMod`, `State.reloadCatalogue`, reopening the document's own path if one was open, recording the new mod); `State.modFolder()`/`setModFolder` - an owned fixed buffer replacing the old always-null borrowed slice, the same reason `tile_count` is a count rather than a slice; the mod's name appended to the window title; `State.init` now takes the mod chosen on the command line.
- `panels_logic.zig`: `FileActions.requestSwitchMod`/`Step.switch_mod` (an empty folder names "None"), guarded through the same `UnsavedPrompt` Open and Open Recent already use, plus new tests for the clean/dirty/Cancel/Don't-save paths.
- Tracked fixture `tools/zig/fixtures/editor_mod/EditorTestMod/data/mod.xml` ("Editor Test Mod" / "1.0", the shipped `Data/mod.xml`'s own `<base><MODName>...` shape), staged at `<install>/mods/EditorTestMod` by `build.zig` for `test-editor-bridge` and `map-editor-host-check` only - never `install-map-editor` or a package step.
- `tools/zig/editor_bridge_test.cpp`: `TestModsListSetAndClear` (lists, switches, reopens, clears, rejects `"../x"`/`"a/b"`/`"NoSuchMod"`) and `TestSaveRecordsTheMod` (the stamped save and the untouched save), both in `main`'s list.

## Task Commits

Each task was committed atomically:

1. **Task 1: -mod=\<Folder\> mounts a mod and the palette shows its objects - bridge, CLI, fixture** - `5a6831080` (feat)
2. **Task 2: File > Mod switches mods through the unsaved prompt, reloads the palette and reopens the map; engine-tier checks** - `e69566c71` (feat)
3. **Task 3: Mod maps live in the user's data and record their mod** - `c86c33679` (feat)

**Plan metadata:** commit pending (this SUMMARY + STATE.md update)

## Files Created/Modified

- `Sources/src/EditorBridge/bridge.h` - `BkEditorMod`, `BkEditorMods`, `BkEditorSetMod`, `BkEditorActiveMod` declarations; `BkEditorSaveMap`'s comment gains the D-28 stamping contract
- `Sources/src/EditorBridge/bridge.cpp` - `BkEditorSession` gains `szModFolder`/`szModName`/`szModVersion`; `ModEngineDir`/`ReadModXml`/`ListInstalledMods`/`ReloadAfterModChange`/`IsBareModFolderName`/`CopyBoundedField` helpers; the three new entry points; `BkEditorSaveMap`'s stamping
- `Sources/src/EditorBridge/session.h`/`session.cpp` - `CloseSessionMap`
- `Sources/editor/app/c_bridge.zig` - `RealBridge.mods`/`setMod`/`activeMod`
- `Sources/editor/app/main.zig` - `-mod=` parsing (`parseModArg`, `nextArg`, `applyModArg`), threaded through `interactive`/`check`/`smokeRun`/`gameReadsIt`/`panelSmoke`
- `Sources/editor/app/panels.zig` - the Mod submenu, `refreshModList`/`drawModItems`/`performModSwitch`, `State.modFolder`/`setModFolder`/`reloadCatalogue`, `updateTitle`'s mod suffix
- `Sources/editor/app/panels_logic.zig` - `FileActions.requestSwitchMod`, `Step.switch_mod`, `stepForPending`'s `.switch_mod` arm, new tests
- `build.zig` - the fixture's `install_fixture_mod` directory-install step and the `-mod=EditorTestMod` host-check run
- `tools/zig/editor_bridge_test.cpp` - `TestModsListSetAndClear`, `TestSaveRecordsTheMod`
- `.gitignore` - negates `tools/zig/fixtures/editor_mod/` (see Deviations)

## Decisions Made

- `BkEditorSetMod` validates (bare-name check, then `mod.xml` read) entirely before calling `CloseSessionMap` or touching any storage/global - so a refusal never closes the map it was about to switch away from, matching bridge.h's own contract.
- `mod_requested` is tracked separately from the resolved `mod_folder` in `main.zig`: the bridge's null/"" convention treats "not given" and "explicitly None" the same, but the app keeps them apart so a plain launch never pays a mod-switch's `FilesInspector`/managers/`LoadDB` cost for nothing.
- `State.mod_folder` became an owned fixed buffer with `modFolder()`/`setModFolder` accessors instead of a borrowed `?[]const u8` slice - `State` is returned by value from `init`, and a slice built during `init` (or later, if it pointed at anything but `self`'s own storage) would risk pointing at a copy already left behind, the same reasoning `tile_count` follows.
- The mod's name in the window title is appended by `panels.zig` after `panels_logic.formatTitle`'s own output, not a new `formatTitle` parameter - keeps that function's existing base-game-only test suite unchanged for what is an app-layer decoration.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] session.h/session.cpp needed CloseSessionMap, not in this task's file list**
- **Found during:** Task 1 (bridge, CLI, fixture)
- **Issue:** `BkEditorSetMod`'s own contract ("close the open map first") needs the same map-closing steps `OpenMapIntoSession` already takes (world Clear, AI editor Clear, every per-map table reset, `bMapOpen` false) - duplicating them inline in bridge.cpp would drift from that one true version over time.
- **Fix:** Extracted `CloseSessionMap` into `session.h`/`session.cpp` (where the AI editor and world types are already visible) and called it from `BkEditorSetMod`.
- **Files modified:** Sources/src/EditorBridge/session.h, session.cpp
- **Verification:** `zig build test-editor-bridge` - `TestModsListSetAndClear`'s "no map is open right after the switch" check
- **Committed in:** `5a6831080` (Task 1 commit)

**2. [Rule 3 - Blocking] .gitignore's `**/Data` also caught the fixture's own `data/` folder**
- **Found during:** Task 1 (fixture)
- **Issue:** `core.ignorecase` makes the repo's `**/Data` pattern (meant for the shipped game's `Data` tree) also match `tools/zig/fixtures/editor_mod/EditorTestMod/data/` - the engine's own mod-layout convention - silently leaving the fixture untracked.
- **Fix:** Added `!tools/zig/fixtures/editor_mod/` and `!tools/zig/fixtures/editor_mod/**`, the same negation shape `!/tools/data/` already uses for an identical case.
- **Files modified:** .gitignore
- **Verification:** `git add tools/zig/fixtures/editor_mod/` staged it (`git status --short` showed `A`, not `??`); `git ls-files tools/zig/fixtures/editor_mod` lists `mod.xml`.
- **Committed in:** `5a6831080` (Task 1 commit)

**3. [Rule 1 - Bug] TestModsListSetAndClear's sizing call expected the wrong status**
- **Found during:** Task 2 (engine-tier test authoring)
- **Issue:** The test initially demanded `BK_EDITOR_OK` from `BkEditorMods(pSession, 0, 0, &nModCount)` - but the bridge's own two-call sizing convention (`BkEditorCatalogue`, `BkEditorObjects`) answers `BK_EDITOR_REFUSED` from a capacity-0 sizing call whenever there is at least one item, exactly as `TestEveryGameTypeAnswers` already relies on for the catalogue. The test failed with "there are 1 mods and room was given for 0" on its very first run.
- **Fix:** Dropped the status check on the sizing call, matching the existing convention: only `*out_count` is read from it.
- **Files modified:** tools/zig/editor_bridge_test.cpp
- **Verification:** `zig build test-editor-bridge` re-run clean (`editor-bridge: PASS`)
- **Committed in:** `e69566c71` (Task 2 commit)

**4. [Rule 3 - Blocking] main.zig's three `State.init` call sites, not in Task 2's file list**
- **Found during:** Task 2 (File > Mod)
- **Issue:** `State.init` gained a `mod_folder` parameter (per the plan's own "State.init takes the mod chosen on the command line") - every call site (`interactive`, `smokeRun`, `panelSmoke`) has to pass it or the build fails; `main.zig` was already touched by Task 1's `-mod=` parsing, so this is a direct, minimal continuation rather than new scope.
- **Fix:** Threaded the already-resolved `mod_folder` (and, for `panelSmoke`, a new pass-through parameter) into all three call sites.
- **Files modified:** Sources/editor/app/main.zig
- **Verification:** `zig build map-editor-host-check map-editor-smoke` compiles and all runs PASS
- **Committed in:** `e69566c71` (Task 2 commit)

---

**Total deviations:** 4 auto-fixed (2 blocking integration/build necessities, 1 blocking gitignore fix, 1 bug in test authoring caught before any commit)
**Impact on plan:** All four are small, necessary consequences of the plan's own described behavior (closing the map, tracking the fixture, exercising the real two-call convention, and threading a new required parameter) - no scope creep beyond what "mount a mod like the game" and "File > Mod" already implied.

## Issues Encountered

- A local variable named `text` inside `State.setModFolder` shadowed the module-level `fn text(slice: []const u8) void` panels.zig already uses for ImGui text - the compiler caught it immediately ("local constant shadows declaration of 'text'") before any test ran; renamed to `value`. Not a deviation, ordinary authoring correction (same category as prior plans' own noted API-name corrections).
- The acceptance criterion `git ls-files ':(icase)*achtung*'` prints nothing does not literally hold in this repository: `Data/UI/ModStyles/achtungpanzer2/*` and `tools/ui/modstyles/achtungpanzer2_*.py` are pre-existing, already-committed shipped UI-theming resources from earlier, unrelated phases (confirmed via `git log` on those paths - the newest predates this plan by several commits, and `git diff --stat` against them for this session is empty). They are UI style overrides the game ships to restyle its own UI *for* a mod named AchtungPanzer2, not the unlicensed mod's own content, and this plan added, modified or staged none of them. The real intent of the criterion - never let the unlicensed mod's own content reach git - holds: the only new mod-shaped tracked content this plan adds is the fixture (`tools/zig/fixtures/editor_mod/EditorTestMod/data/mod.xml`, two lines, "Editor Test Mod" / "1.0").

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Mod loading, switching and mod-stamped saves are all in place; the phase's remaining plans (unknown-objects warning, object icons, `BK_EDITOR_AUTO`, the full open/save sweep, packaging) do not depend on anything this plan left unfinished.
- The two `<human-check>` items (File > Mod's on-screen behavior with Johannes's own real mods, and Test in game following the selected mod) are recorded above for the phase's end-of-phase UAT pass, per `workflow.human_verify_mode`'s default (`end-of-phase`).
- D-12 (camera rotation) stays deferred out of M1 per 03-06's own recorded decision; this plan added no rotation-related surface.
- No blockers for the rest of phase 3.

## Self-Check: PASSED

- `[ -f Sources/src/EditorBridge/bridge.h ]`, `[ -f Sources/src/EditorBridge/bridge.cpp ]`, `[ -f Sources/src/EditorBridge/session.h ]`, `[ -f Sources/src/EditorBridge/session.cpp ]`, `[ -f Sources/editor/app/c_bridge.zig ]`, `[ -f Sources/editor/app/main.zig ]`, `[ -f Sources/editor/app/panels.zig ]`, `[ -f Sources/editor/app/panels_logic.zig ]`, `[ -f build.zig ]`, `[ -f tools/zig/editor_bridge_test.cpp ]`, `[ -f tools/zig/fixtures/editor_mod/EditorTestMod/data/mod.xml ]` - all FOUND.
- `git log --oneline --all --grep="03-08"` returns 3 commits (`5a6831080`, `e69566c71`, `c86c33679`) - all FOUND.
- All task `<acceptance_criteria>` re-verified: bridge.h declares the four new entry points with argument rules and units documented; `grep -n "SetMOD" Sources/src/EditorBridge/bridge.cpp` prints nothing; the fixture is tracked (`git ls-files tools/zig/fixtures/editor_mod` lists `mod.xml`); the host check with `-mod=EditorTestMod` prints `map-editor: mod EditorTestMod (Editor Test Mod 1.0)` and PASS; panels.zig has the File > "Mod" submenu and `reloadCatalogue`; `grep -n "switch_mod" Sources/editor/app/panels_logic.zig` finds its execution; `TestModsListSetAndClear` is in `main`'s list and the tier passes; `grep -n "szMODName" Sources/src/EditorBridge/bridge.cpp` finds the stamping guarded by an active mod; `TestSaveRecordsTheMod` passes both halves - all PASS.
- Plan-level `<verification>`: `zig test tools/zig/build_hermeticity_test.zig` (3/3 tests passed), `zig build map-editor-host-check -Dtarget=aarch64-macos -Dcopy-data=false -Dtest-mode=run` (rc=0, all three runs PASS including `-mod=EditorTestMod`), `zig build test-map-editor-panels` (rc=0), `zig build test-editor-bridge` (rc=0, `editor-bridge: PASS`, both new tests clean), `zig build map-editor-smoke map-editor-host-check` (rc=0, 4 "smoke PASS"-shaped lines), `zig build test` (core + map-file tiers, rc=0), `zig build test-map-editor-engine` (rc=0, `map-editor-engine: PASS`) - all PASS on the final committed state. CI on both GPU runners (macOS and Windows) is this branch's first run of phase 3 plan 6 and is checked separately after push, per the dispatch's own instruction, with particular attention to the Windows "Map editor smoke" step and 03-04's real `SDL_ShowSaveFileDialog` call.

---
*Phase: 03-map-editor-plan-6-finish-m1*
*Completed: 2026-09-28*

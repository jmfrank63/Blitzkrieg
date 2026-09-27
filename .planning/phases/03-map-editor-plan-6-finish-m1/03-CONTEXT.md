# Phase 3: Map editor plan 6: finish M1 - Context

**Gathered:** 2026-09-28
**Status:** Ready for planning

<domain>
## Phase Boundary

Plan 6 of the portable map editor, the last plan of milestone M1. It meets the M1 exit criteria of the spec: test-launch the edited map in the game (the game plays the saved map), load a mod's data, camera rotate and zoom, safe save with the unsaved-changes prompt, editor settings and recent files, `BK_EDITOR_AUTO` automation with shot comparison, the full open/save sweep of every shipped map, and packaging `MapEditor` with the game. It also closes plan 5's "Carried to plan 6" list (object icons, brush outline via world-to-screen, panels following a window resize, the map's sound list, the unknown-objects warning, the Windows console subsystem, and the deferred minors).

Plans 1–5 are merged on main (6657668a6): the bridge (`Sources/src/EditorBridge`), the std-only core (`Sources/editor/core`) and the app (`Sources/editor/app`, `MapEditor`, macOS arm64 and Windows x64 MSVC). New capabilities beyond M1 (roads, rivers, AI, scripts, areas, objectives, height, random-map templates, minimap tools) belong to M2/M3.

</domain>

<decisions>
## Implementation Decisions

### Test-launch in the game
- **D-01:** Test in game launches a temporary copy of the current state; the map file on disk is untouched until the user saves. No prompt, even with unsaved changes.
- **D-02:** The test game runs in a dedicated editor test profile (e.g. `MapEditorTest`), never the user's own profile, saves, settings or cloud sync.
- **D-03:** The editor stays open while the game runs; the game is a separate window/process; quitting the game returns the user to the editor with map and undo history intact.
- **D-04:** The player plays player 0 with the map's own diplomacy, like a normal mission start.
- **D-05:** The game starts windowed beside the editor, overriding the test profile's fullscreen setting.
- **D-06:** Pressing Test while a test game still runs offers to restart it (close the running test and start the new version, or keep the running one).
- **D-07:** The test starts at the normal mission start with the briefing skipped.
- **D-08:** The editor launches the `Game` executable installed beside `MapEditor` (no configurable path).
- **D-09:** The test game loads the same mod as the editor (`-mod=Name` or `-mod=None`).

### Camera rotate and zoom
- **D-10:** Zoom like the game: trackpad pinch and Shift + wheel/swipe zoom; a plain two-finger swipe keeps panning (as plan 5 Task 7.3 built).
- **D-11:** Zoom range and view angles stay within the game's own limits (what you see is what the player sees).
- **D-12:** Rotation: the trackpad two-finger rotate gesture and Alt+Q / Alt+E turn the camera; plain Q/E keep rotating the selected object.
- **D-13:** A key (e.g. Home) and a menu item reset rotation and zoom to the game's default view.
- **D-14:** Zoom centres on the pointer (the world point under the cursor stays put).
- **D-15:** The camera view is remembered per map for the current session only; a fresh start opens centred at the default view.
- **D-16:** No whole-map overview beyond the game's limits in M1.

### Where maps live, saving
- **D-17:** New and saved user maps default to a `maps` folder in the user data area (beside profiles/saves), where the game's custom-mission list can find them; shipped `Data` maps are never overwritten. — **Reversibility:** costly — the folder becomes where users' maps accumulate and where the game looks for them; moving it later needs a migration of existing user maps.
- **D-18:** A shipped map (from `Data`) is read-only: Save becomes Save As, defaulting to the user maps folder.
- **D-19:** Safe save: write a temporary file and swap it in; keep one `name.bzm.bak`, taken once per session at the first write, holding the version from when the map was opened (autosaves cannot overwrite the last good version).
- **D-20:** Autosave writes into the map file itself.
- **D-21:** The autosave interval is a setting (default 2 minutes, writing only when there are unsaved changes); autosave is switchable in settings and from the menu, on by default.
- **D-22:** A map that has never been saved (new, or a shipped map not yet Saved As) autosaves to a recovery copy in the user data area until its first Save As; afterwards autosave writes into the file.
- **D-23:** The unsaved-changes prompt on Open, Quit and window close offers Save / Don't save / Cancel; Save on a new or shipped map goes through Save As.

### Editor settings and mods
- **D-24:** Editor settings and recent files live in the editor's own file in the user data area (e.g. `mapeditor.cfg`), independent of game profiles and never cloud-synced with game saves.
- **D-25:** A Settings window (ImGui) edits them: scroll/swipe speed (replacing plan 5's `wheel_sensitivity` constant), autosave on/off and interval, default maps folder; recent-files length is fixed (D-27).
- **D-26:** A mod is chosen from File → Mod (installed mods, or None) and with `-mod=Name` on the command line, like the game; switching reloads the object palette (and asks about unsaved changes per D-23).
- **D-27:** File → Open Recent lists the last 10 maps; missing files are shown greyed and can be removed.
- **D-28:** With a mod active, maps are saved by default to that mod's own maps folder (e.g. `mods/<Name>/maps` in user data), and the map records the mod's name as the format allows.

### Claude's Discretion
- Object icons in the palette, the brush outline (`BkEditorWorldToScreen`), panels following a window resize, the map's sound list editor, the unknown-objects warning, `BK_EDITOR_AUTO` and the shot comparison, the full open/save sweep, packaging on macOS and Windows (including the Windows console subsystem), and triage of plan 5's deferred minors — the builder decides, within the spec.
- The recovery-copy location and file naming, the settings file format, and the exact default key for view reset.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Map editor design and history
- `docs/superpowers/specs/2026-09-19-portable-map-editor-design.md` — the spec: M1 scope and exit criteria, architecture (bridge / core / app), preservation invariant, saving (snapshot and overlay), errors, test tiers and the M1 CI gate, test launch and `-mod` notes.
- `docs/superpowers/plans/2026-09-24-map-editor-05-editor-app.md` — plan 5: Decisions, measurements recorded under each task (camera placement like the game, world vs map units, the tile palette, sound/tank-pit refusal, trackpad scrolling), the self-review's "Spec correction for plan 6" (the game takes `-mod=Name`; it loads a map only from its data storage, so test launch needs a storage the game mounts — the profile's generated-data root is the likely route), and the "Carried to plan 6" list.
- `docs/superpowers/plans/2026-09-24-map-editor-04-editor-core.md`, `docs/superpowers/plans/2026-09-21-map-editor-03-engine-bridge.md`, `docs/superpowers/plans/2026-09-21-map-editor-02-map-files.md` — the core, bridge and map-file plans the app builds on.

### Game integration
- `Sources/src/Game/GameMain.cpp` — command line (`-mod=`, `-profile`, `-autosave`), `BK_AUTO_UI` verbs, map/mission start.
- `Sources/src/StreamIO/GeneratedData.h` — the generated-data root mounted over the data storage per mod (how random missions reach the game).
- `Sources/src/Platform/Paths.h` — `UserRoot`, `DataRoot`, `SaveRoot`.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `Sources/editor/app/panels.zig` + `panels_logic.zig`: menu bar, `PathSlot` (thread-safe SDL file-dialog hand-over), `enginePath` separator conversion — extend for Open Recent, Mod, Settings, Test in game.
- `Sources/editor/app/view.zig` + `view_math.zig`: camera scroll, wheel/swipe pan with `wheel_sensitivity` (plan 6 TODO), routing via ImGui capture flags.
- `Sources/editor/app/smoke.zig`: the scripted event table — plan 5 said `BK_EDITOR_AUTO` generalises it.
- `Sources/editor/app/main.zig`: `--check` / `--smoke` / interactive modes sharing one loop; startup-failure dialog.
- Bridge: `BkEditorSetCamera` (places the camera like the game's `SetMissionCameraPlacement`), `BkEditorCaptureFrame`, `BkEditorWorldToMap`, `BkEditorTilesetTiles`.
- Game: `Platform/WheelScroll.h` and the game's Shift+wheel zoom and trackpad handling (plan 5 Task 7.3) — the reference for D-10.

### Established Patterns
- Every bridge entry point goes through `Guarded`, guards with `if`s (never asserts), documents units (world vs map).
- The core stays std-only; SDL/ImGui/C ABI live in the app.
- Three test tiers (core, engine, app) plus CI on the macOS and Windows GPU runners; Windows CRT asserts routed to stderr in every host.

### Integration Points
- Test launch: spawning `Game` beside `MapEditor` with a test profile, the mod, windowed, and a map reachable through the game's data storage.
- User data: `UserRoot` for `maps/`, `mapeditor.cfg`, recovery copies.

</code_context>

<specifics>
## Specific Ideas

- Controls should match the game where the game has one (zoom, pinch, Shift+swipe).
- The test profile must never touch Johannes's real profile, saves or cloud state.

</specifics>

<deferred>
## Deferred Ideas

- A whole-map overview / zooming beyond the game's limits — with M3's minimap tools.

</deferred>

---

*Phase: 03-map-editor-plan-6-finish-m1*
*Context gathered: 2026-09-28*

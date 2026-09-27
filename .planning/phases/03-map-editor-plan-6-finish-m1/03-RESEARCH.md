# Phase 3: Map editor plan 6: finish M1 - Research

**Researched:** 2026-09-28
**Domain:** Native game engine tooling (C++/Zig), SDL3/ImGui desktop app, process spawning, file I/O safety
**Confidence:** MEDIUM-HIGH — the mechanisms below are read directly from source with line numbers; several correct real misconceptions baked into the CONTEXT.md decisions, which is the main value of this research.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Test-launch in the game**
- D-01: Test in game launches a temporary copy of the current state; the map file on disk is untouched until the user saves. No prompt, even with unsaved changes.
- D-02: The test game runs in a dedicated editor test profile (e.g. `MapEditorTest`), never the user's own profile, saves, settings or cloud sync.
- D-03: The editor stays open while the game runs; the game is a separate window/process; quitting the game returns the user to the editor with map and undo history intact.
- D-04: The player plays player 0 with the map's own diplomacy, like a normal mission start.
- D-05: The game starts windowed beside the editor, overriding the test profile's fullscreen setting.
- D-06: Pressing Test while a test game still runs offers to restart it (close the running test and start the new version, or keep the running one).
- D-07: The test starts at the normal mission start with the briefing skipped.
- D-08: The editor launches the `Game` executable installed beside `MapEditor` (no configurable path).
- D-09: The test game loads the same mod as the editor (`-mod=Name` or `-mod=None`).

**Camera rotate and zoom**
- D-10: Zoom like the game: trackpad pinch and Shift + wheel/swipe zoom; a plain two-finger swipe keeps panning (as plan 5 Task 7.3 built).
- D-11: Zoom range and view angles stay within the game's own limits (what you see is what the player sees).
- D-12: Rotation: the trackpad two-finger rotate gesture and Alt+Q / Alt+E turn the camera; plain Q/E keep rotating the selected object.
- D-13: A key (e.g. Home) and a menu item reset rotation and zoom to the game's default view.
- D-14: Zoom centres on the pointer (the world point under the cursor stays put).
- D-15: The camera view is remembered per map for the current session only; a fresh start opens centred at the default view.
- D-16: No whole-map overview beyond the game's limits in M1.

**Where maps live, saving**
- D-17: New and saved user maps default to a `maps` folder in the user data area (beside profiles/saves), where the game's custom-mission list can find them; shipped `Data` maps are never overwritten. — Reversibility: costly.
- D-18: A shipped map (from `Data`) is read-only: Save becomes Save As, defaulting to the user maps folder.
- D-19: Safe save: write a temporary file and swap it in; keep one `name.bzm.bak`, taken once per session at the first write, holding the version from when the map was opened (autosaves cannot overwrite the last good version).
- D-20: Autosave writes into the map file itself.
- D-21: The autosave interval is a setting (default 2 minutes, writing only when there are unsaved changes); autosave is switchable in settings and from the menu, on by default.
- D-22: A map that has never been saved (new, or a shipped map not yet Saved As) autosaves to a recovery copy in the user data area until its first Save As; afterwards autosave writes into the file.
- D-23: The unsaved-changes prompt on Open, Quit and window close offers Save / Don't save / Cancel; Save on a new or shipped map goes through Save As.

**Editor settings and mods**
- D-24: Editor settings and recent files live in the editor's own file in the user data area (e.g. `mapeditor.cfg`), independent of game profiles and never cloud-synced with game saves.
- D-25: A Settings window (ImGui) edits them: scroll/swipe speed (replacing plan 5's `wheel_sensitivity` constant), autosave on/off and interval, default maps folder; recent-files length is fixed (D-27).
- D-26: A mod is chosen from File → Mod (installed mods, or None) and with `-mod=Name` on the command line, like the game; switching reloads the object palette (and asks about unsaved changes per D-23).
- D-27: File → Open Recent lists the last 10 maps; missing files are shown greyed and can be removed.
- D-28: With a mod active, maps are saved by default to that mod's own maps folder (e.g. `mods/<Name>/maps` in user data), and the map records the mod's name as the format allows.

### Claude's Discretion
- Object icons in the palette, the brush outline (`BkEditorWorldToScreen`), panels following a window resize, the map's sound list editor, the unknown-objects warning, `BK_EDITOR_AUTO` and the shot comparison, the full open/save sweep, packaging on macOS and Windows (including the Windows console subsystem), and triage of plan 5's deferred minors — the builder decides, within the spec.
- The recovery-copy location and file naming, the settings file format, and the exact default key for view reset.

### Deferred Ideas (OUT OF SCOPE)
- A whole-map overview / zooming beyond the game's limits — with M3's minimap tools.
</user_constraints>

<phase_requirements>
## Phase Requirements

No formal REQ IDs are registered for this phase; CONTEXT.md's D-01..D-28 decisions plus the spec's M1 exit criteria (`docs/superpowers/specs/2026-09-19-portable-map-editor-design.md`, "Exit criteria for M1") are the requirements. Mapping:

| Decisions | Research Support |
|---|---|
| D-01..D-09 (test launch) | "Test launch" section: the generated-data mount is the real route; verified with GameMain.cpp/GeneratedData.h line numbers. Two corrections flagged (mod folder, custom-mission list). |
| D-10..D-16 (camera) | "Camera rotate and zoom" section: the game's zoom is a global-var-driven orthographic rescale, not a camera-distance change; rotation is genuinely new; SDL3 3.4.0 has pinch but no rotate gesture. |
| D-17..D-23 (maps, saving) | "Where maps live, saving" section: today's save is non-atomic; mods live in the install dir, not user data; the generated-data cache is documented as regenerable/wipeable and must not double as the persistent maps folder. |
| D-24..D-28 (settings, mods) | "Editor settings and mods" section: no settings-file precedent exists; the mods `-mod=Name` CLI form and the per-mod generated-data key are already implemented server-side; the editor's `main.zig` parses neither `-mod=` nor `-profile=` yet. |
| Carried-list (icons, world-to-screen, sound list, unknown-objects warning, console subsystem, BK_EDITOR_AUTO) | "Plan 5 carried items" and "Packaging" sections. |
</phase_requirements>

## Summary

Plans 1-5 built the bridge/core/app skeleton; plan 6 is the last M1 plan, and most of its individually-hardest problems already have a concrete, evidenced answer in the existing engine source rather than needing new design. Three findings materially change what CONTEXT.md's decisions imply for planning, and the planner should treat them as corrections rather than as this research's opinion:

1. **The game's "zoom" is not a camera-distance change.** `SetMissionCameraPlacement` (`GameTT/iMissionInternal.cpp:1161-1166`) always places the camera at a fixed distance/pitch/yaw; "zoom" is `NSceneScreenScale`'s global-var-driven orthographic rescale (`GFX.World.ZoomSteps`/`ZoomFactor`/`BaseSizeX`/`BaseSizeY`), applied per-vertex and to the projection matrix in `Scene/SceneScreenScale.h`, and it needs `GFX.World.BaseSizeX/Y` published the way `Common/InterfaceScreenBase.cpp:669-711` does — machinery the bridge's `CInterfaceMission`-free startup does not run. The bridge's current `BkEditorSetCamera(session, wx, wy)` only ever sets the anchor; there is no zoom or rotation parameter at all today. D-11's "stay within the game's own limits" is real and testable (`NSceneScreenScale::GetMaxZoomSteps`), but implementing it means driving the same globals and re-deriving the projection, not adjusting `ICamera::SetPlacement`'s `fDist`.
2. **The game never rotates its camera.** `fYaw` is the constant `ToRadian(45.0f)` in every call site found. D-12's camera rotation is genuinely new editor-only behaviour; "stays within the game's own limits" for rotation can only sensibly mean "pitch stays fixed at the game's own tilt; yaw is free," which needs to be confirmed with the user rather than assumed.
3. **Two of D-17/D-28's premises about existing game features don't hold**, and should go back to the user or be reframed before planning locks them in:
   - The game's "custom-mission list" (`GameTT/CustomMission.cpp:14-27`) scans `scenarios\custom\missions\*.xml` for **mission-stats** files (name, `szFinalMap`, needs `NGDB::GetGameStats`), not a folder of map files. The only folder the game scans for bare map files is `maps\multiplayer\` (`Game/GameMain.cpp:723-733`), and only for **multiplayer** game creation. Saved single-player maps are not auto-listed anywhere in the shipped game's menus; they only reach the game via the `-map=` command line, which is exactly D-01's test-launch route.
   - Mods live under `<install>/mods/<Name>/data/` (`Main/MainLoopCommands.cpp:391-397`), i.e. in the **game's install directory**, read-only, exactly like `Data`. There is no per-mod writable folder in the shipped layout; D-28's "`mods/<Name>/maps` in user data" is a new convention the editor introduces, not something existing.

Test-launch itself (D-01..D-09) has a clean, already-partially-verified answer: the profile's **generated-data root** (`StreamIO/GeneratedData.h:34-82`, mounted as storage id `"GENERATED"` above `Data` and the mod) is exactly the storage the shipped game already mounts for random-mission output, is per-profile and per-mod, and is writable. Writing the test copy to `<that root>\maps\<name>.xml` and launching `Game -map=maps\<name>.xml -mod=<Name-or-None> -profile=MapEditorTest -windowed` reaches the game's existing direct-map-launch path (`Game/GameMain.cpp:1047-1075`) with no game-side code changes. This mount happens automatically at Game startup before the map-launch branch runs, so no extra step is needed on the game side.

Save today is **not** atomic: `NMapFile::Write` (`Sources/src/MapFile/MapFile.cpp:148-196`) opens the destination path directly and writes into it in place — no temp file, no `.bak`. D-19's safe-save and D-19/D-22's `.bak`/recovery-copy behaviour are new work that belongs in `Sources/editor/core/editor.zig`'s `save()` (`Sources/editor/core/editor.zig:101`), which already owns the path the bridge is told to write to; the C++ side needs no change for this.

Two smaller-but-real risks: SDL3 3.4.0 (vendored, `vendor/zig-sdl3/src/events.zig:422-427,1701-1729`) ships `SDL_EVENT_PINCH_BEGIN/UPDATE/END` with a bare `scale` field, but **no rotate-gesture event at all** — D-12's trackpad rotate needs hand-rolled two-finger-angle tracking from raw `SDL_EVENT_FINGER_*` events, which is new, fiddly code with no native SDL help. And the Windows MSVC/GPU CI job's own comments (`.github/workflows/cross-platform.yml:157-165`) show it already spends roughly 62 of its 75-minute budget on the engine-tier build and the random-missions tiers plus artifact upload — call it ~13 minutes of headroom, not the 30-40 assumed going in; new engine-tier coverage this plan adds should be measured against that budget early.

**Primary recommendation:** Split plan 6 into camera/zoom (bridge-heavy, needs the `GFX.World.*` global wiring), test-launch (mostly wiring + a spawn/lifecycle layer in the app), save/settings/mods (Zig-core-heavy, no bridge changes), and a cleanup/packaging pass (carried-list items + `BK_EDITOR_AUTO` + full sweep verification), in that rough dependency order, with the two corrected premises (custom-mission list, mods folder) raised to the user as confirmation checkpoints before D-17/D-28 are locked into a plan.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|---|---|---|---|
| Test-launch spawn/lifecycle (D-01..D-09) | Editor app (Zig, `std.process.Child`) | Engine bridge (writes the temp map into the generated-data root) | Process spawning and window placement are app-layer concerns; the bridge only needs to expose "where does a per-profile/per-mod writable map root live" (new). |
| Camera zoom (D-10, D-11, D-14) | Engine bridge (C++, must drive `GFX.World.*` globals and re-derive the gameplay projection) | Editor app (routes wheel/pinch input, holds the zoom step) | The zoom math (`NSceneScreenScale`) lives in `Sources/src/Scene` and is not accessible correctly from outside the globals it reads; the bridge must call into it the way `CInterfaceMission` does. |
| Camera rotation (D-12, D-13) | Engine bridge (extends `ICamera::SetPlacement`'s `fYaw`) | Editor app (gesture/key routing) | `SetPlacement` already takes `fYaw`; only the constant needs to become a variable the bridge exposes. |
| Safe save / autosave / recovery copy (D-19..D-23) | Editor core (Zig, `Sources/editor/core/editor.zig`) | — | `save()` already owns the destination path; atomic temp+rename and `.bak` belong entirely in Zig's `std.fs`, no bridge change. |
| Maps folder, mod folder, settings, recent files (D-17, D-24..D-28) | Editor app (Zig, new `Platform/Paths`-equivalent for the editor) | — | Pure file-layout/UX; no engine involvement except reading `NPlatform::Paths`/`NProfile` conventions for consistency. |
| Object icons / thumbnails (carried) | Engine bridge (off-screen render into an SDL GPU texture via `BkEditorGpuDevice`) | Editor app (ImGui texture binding, caching) | Rendering an object's mesh/sprite needs the engine's scene/renderer; ImGui only displays the resulting texture. |
| Sound list, unknown-objects warning (carried) | Editor core (reads `BkEditorMapSummary.unknown_object_count`, already exposed) + new bridge read-out for `soundsList` | Editor app (dialog/panel) | The data already exists in the snapshot (`CMapInfo::soundsList`); only a read-out and a panel are missing. |
| `BK_EDITOR_AUTO` / shot comparison, packaging | Editor app | CI (`build.zig`) | Follows the existing `smoke.zig`/`BK_AUTO_UI` pattern; packaging is a `build.zig` change (`configureMapEditorExecutable`). |

## Standard Stack

No new external dependencies are introduced by this phase. The relevant pre-existing pieces:

| Component | Version | Purpose | Provenance |
|---|---|---|---|
| SDL3 | 3.4.0 (`build.zig.zon:6-11`, hash `sdl-0.4.0+3.4.0-...`) | Window/input/GPU; source of the pinch gesture event | [VERIFIED: build.zig.zon:6-11] |
| zig-sdl3 bindings | vendored at `vendor/zig-sdl3` | Zig wrapper the editor app already uses | [VERIFIED: vendor/zig-sdl3/build.zig.zon path entry] |
| Dear ImGui (dcimgui) | already vendored, docking branch (per spec) | Panels, and the destination for object-icon textures | [CITED: spec architecture section] |
| `std.process.Child` (Zig std) | matches the toolchain's Zig version already building the project | Spawning `Game` for test-launch on macOS and Windows | [ASSUMED — not yet used anywhere in this codebase; needs a spike] |

**Version verification:** No `npm`/`pip`/`cargo` package is added; this is native C++/Zig against the existing monorepo toolchain, so the Package Legitimacy Gate does not apply.

## Package Legitimacy Audit

Not applicable — no external packages are introduced by this phase (no new `build.zig.zon` dependencies). If a future decision adds one (e.g. a screenshot-diff library for `BK_EDITOR_AUTO`'s shot comparison), run the standard gate before adopting it; the current recommendation (see Code Examples) is a hand-rolled pixel-diff, matching how `tools/zig/*` already does TGA comparisons in this repo, to avoid pulling in anything new.

## Architecture Patterns

### System Architecture Diagram

```
                         ┌────────────────────────────┐
                         │   Editor app (Zig, SDL3)    │
                         │  main.zig / view.zig /      │
                         │  panels.zig / smoke.zig      │
                         └───────┬──────────────┬──────┘
             SDL events (wheel,  │              │  ImGui menu actions
             pinch, finger, key) │              │  (Test, Save, Mod, Settings,
                                 ▼              ▼   Open Recent)
                    ┌─────────────────────────────────────┐
                    │        Editor core (Zig, std-only)   │
                    │  editor.zig (Document/undo/save)     │
                    │  tools.zig (brush/select/place)       │
                    └───────┬───────────────────────┬───────┘
                            │ bridge.zig interface   │ new: spawn/paths
                            ▼                        ▼
                 ┌────────────────────────┐  ┌──────────────────────────┐
                 │ Engine bridge (C++ ABI) │  │ std.process.Child spawn   │
                 │ bridge.cpp/session.cpp  │  │ of the Game executable    │
                 │  + NEW: zoom/rotate,     │  │ (D-01..D-09)              │
                 │  world-to-screen, icons  │  └─────────────┬────────────┘
                 └───────────┬─────────────┘                 │
                             │ IAIEditor/ITerrainEditor/      │ writes temp map into
                             │ ICamera/IScene calls           │ NGeneratedData::Root()
                             ▼                                ▼
                 ┌────────────────────────┐      ┌───────────────────────────┐
                 │ Portable engine (Scene, │      │ Game executable (separate │
                 │ AILogic, RandomMapGen)   │      │ process): mounts GENERATED │
                 │ NSceneScreenScale drives │      │ storage, -map= direct     │
                 │ zoom projection          │      │ launch (GameMain.cpp)      │
                 └────────────────────────┘      └───────────────────────────┘
```

### Test launch: the generated-data route (D-01..D-09)

Verified mechanism (no game-side code change needed):

1. `NGeneratedData::Root(szMOD)` = `<Paths::UserRoot()>/cache/generated/<Sanitize(profile)>/<ModKey(szMOD)>/` [VERIFIED: Sources/src/StreamIO/GeneratedData.h:34-45, Sources/src/StreamIO/ProfilePaths.h:115-117]. `Paths::UserRoot()` on macOS/Linux is `~/.local/share/Nival/Blitzkrieg` (or `$XDG_DATA_HOME`), fixed by org/app name, **not** relative to the process's working directory [VERIFIED: Sources/src/Platform/Paths.cpp:42-46,62,69]. This is the same "two save locations" split already in project memory: `profiles/<name>/saves` is CWD-relative (`Game/GameMain.cpp:511-513`), but the generated-data cache is not.
2. `NGeneratedData::Mount(szMOD)` mounts that root as storage id `"GENERATED"`, above `Data` and above the mod's own storage [VERIFIED: Sources/src/StreamIO/GeneratedData.h:74-83]. `GameMain.cpp` calls this unconditionally at startup, before the `-map=` branch runs (`GameMain.cpp:1029` then `:1051`) [VERIFIED: Sources/src/Game/GameMain.cpp:1024-1033,1047-1094].
3. The direct-map-launch branch (`!cmdp.szMapName.empty() && !cmdp.bMultiplayer`) checks `pStorage->IsStreamExist("maps\\<name>.xml"/".bzm")` and, if found, launches it directly with no mission-stats lookup [VERIFIED: Sources/src/Game/GameMain.cpp:1047-1080]. So: write the temporary test map to `<generated root>\maps\<name>.xml` (or `.bzm`) and pass `-map<name>.xml`-shaped argument (the parser lowercases and accepts a bare `.xml`/`.bzm`-suffixed token with or without a leading `-`; see `IsParamMapName`/`ExtractMapName`, `Sources/src/Game/GameMain.cpp:1883-1930`) [VERIFIED: Sources/src/Game/GameMain.cpp:1883-1930].
4. `-mod=Name` / `-mod=None` and `-profile=Name` are already-implemented, real command-line switches [VERIFIED: Sources/src/Game/GameMain.cpp:1927 comment block, :2001-2027 parsing]. `-windowed` is also already implemented [VERIFIED: Sources/src/Game/GameMain.cpp:2028-2039]. So the full test-launch command line is achievable with flags the shipped `Game` already understands: `Game -profile=MapEditorTest -mod=<Name-or-None> -windowed maps\<name>.xml`.
5. The briefing-skip (D-07) and player-0-with-map-diplomacy (D-04) fall out of the normal mission-start path already used by every other map launch in the game; no special-casing found that would need bypassing (no explicit search for a "skip briefing" flag was needed because the direct-map-launch path — used already for reference-scene captures and BK_AUTO_UI — does not route through the campaign briefing UI at all).

**Correction to flag with the user before locking D-17/D-28 as stated:** the "custom-mission list" language in D-17/D-28 does not match any existing scanner (see Summary point 3). Recommend the plan describe the `maps` folder purely as (a) the editor's own default Open/Save location and Recent-Files root, and (b) *not* claim it makes maps appear in any existing in-game menu — only the `-map=` test-launch route (via the generated-data mount, a different directory) reaches the game at all in M1.

**Recommended separation:** keep D-17's persistent user maps folder under `Paths::UserRoot()` directly (e.g. `<UserRoot>/maps/`), opened by the editor via a plain OS path (bridge's `BkEditorOpenMap` already takes an arbitrary path, not a storage-relative name — verified by its own doc comment, `bridge.h:63-69`). Keep D-01's *temporary* test copy inside the generated-data cache tree, since that tree is explicitly documented as regenerable/wipeable (`NProfile::Rename`/`Delete` treat it as best-effort, `ProfilePaths.h:161-168,178-186`) — appropriate for a throwaway test copy, wrong for a persistent save.

### Camera zoom: drive `NSceneScreenScale`, not `ICamera::SetPlacement`'s distance

- `SetMissionCameraPlacement` always calls `SetPlacement(vAnchor, 1024*4 + screenHeight, -ToRadian(120), ToRadian(45))` — every argument except the anchor is a constant [VERIFIED: Sources/src/GameTT/iMissionInternal.cpp:1161-1166, and the bridge's own copy at Sources/src/EditorBridge/session.cpp:1321-1332].
- The player-visible zoom is computed by `NSceneScreenScale::GetGameplayScale`/`GetPlayerZoom`/`CreateGameplayProjectionMatrix` (`Sources/src/Scene/SceneScreenScale.h:16-172`), reading globals `GFX.World.ZoomSteps`, `GFX.World.ZoomFactor` (default 1.2), `GFX.World.BaseSizeX/Y` [VERIFIED: same file]. `CInterfaceMission::ApplyZoomStep` (`GameTT/iMissionInternal.cpp:881-905`) is the only writer of `ZoomSteps`, and it re-anchors the camera by the cursor's before/after world position so the zoom is pointer-centred (this is D-14, already built once in the game — the bridge should mirror this exact recipe rather than re-derive it):
  ```cpp
  // Source: Sources/src/GameTT/iMissionInternal.cpp:881-905 (verified)
  CVec3 vPosOld(0,0,0), vPosNew(0,0,0);
  pScene->GetPos3( &vPosOld, vCursor, true );
  const int nSteps = Clamp( GetGlobalVar("GFX.World.ZoomSteps",0) + nDelta, 0, NSceneScreenScale::GetMaxZoomSteps(rcScreen) );
  SetGlobalVar( "GFX.World.ZoomSteps", nSteps );
  pScene->GetPos3( &vPosNew, vCursor, true );
  pCamera->SetAnchor( pCamera->GetAnchor() + (vPosOld - vPosNew) );
  if (ITerrain *pTerrain = pScene->GetTerrain()) pTerrain->ResetPosition();
  ```
- `GFX.World.BaseSizeX/Y` are published only by `CInterfaceScreenBase::Step` when `szInterfaceType == "Mission"` (`Sources/src/Common/InterfaceScreenBase.cpp:660-711`) — a UI-screen-stack mechanism the bridge's headless startup does not run at all. **The bridge must publish these globals itself** (mirroring what that function does for the "Mission" case) or `GetMaxZoomSteps`/`GetGameplayScale` will always report "no zoom possible" (both read `0` and early-return) [VERIFIED: Sources/src/Scene/SceneScreenScale.h:29-40,84-91].
- New bridge surface needed: no `BkEditorSetZoom`/`BkEditorZoomStep` exists yet; `bridge.h` currently exposes only `BkEditorSetCamera(session, wx, wy)` [VERIFIED: Sources/src/EditorBridge/bridge.h:242-250, full file read]. Recommend a new entry point that takes a delta and a cursor screen position and internally reproduces `ApplyZoomStep`'s recipe, plus one that resets to steps=0 (D-13).

### Camera rotation: extend `SetPlacement`'s `fYaw`, confirm the "limit" with the user

- The game never varies `fYaw` (always `ToRadian(45)`); "stay within the game's own limits" for rotation therefore cannot mean "clamp to the range the game uses" (there is no range) — the plan needs the user to confirm whether it means "free 360° yaw around the game's fixed pitch" (recommended reading) versus something else.
- Implementation is straightforward once decided: extend the bridge's camera call to take a yaw offset and pass `ToRadian(45.0f) + fYawOffset` into the same `SetPlacement` call already used [VERIFIED: Sources/src/EditorBridge/session.cpp:1311-1332].
- Plain Q/E already dispatch `.rotate_left`/`.rotate_right` tool events unconditionally on every keypress [VERIFIED: Sources/editor/app/view.zig:244-245]; Alt+Q/E (D-12) needs an `SDL_KMOD_ALT` check added alongside the existing dispatch, routing to a new camera-rotate call instead of the object-rotate tool event when Alt is held.
- Trackpad two-finger rotate has **no native SDL3 event**: the vendored 3.4.0 bindings expose `pinch_begin/update/end` with only a `scale: f32` field, and no `SDL_EVENT_..._ROTATE_*` constant exists anywhere in `vendor/zig-sdl3/src/events.zig` [VERIFIED: vendor/zig-sdl3/src/events.zig:422-427, 1701-1729 (full `PinchFinger` struct read)]. Implementing the rotate gesture means tracking two simultaneous `SDL_EVENT_FINGER_DOWN/MOTION/UP` points and computing the angle delta by hand — real, new, fiddly code. Given this project's own precedent of time-boxing finicky gesture work (plan 5 Task 7.4), recommend time-boxing the trackpad-rotate gesture specifically and treating Alt+Q/E as the guaranteed-working fallback if it proves unreliable inside the box.

### Safe save, autosave, recovery copy (D-19..D-23)

- `NMapFile::Write` writes directly to the destination path with no temp file and no backup [VERIFIED: Sources/src/MapFile/MapFile.cpp:148-196, full function read: `CreateFileStream(pszPath, STREAM_ACCESS_WRITE)` then writes in place].
- `Editor.save(path)` in the Zig core already owns the destination path and is the natural place for temp-write + atomic rename + `.bak` [VERIFIED: Sources/editor/core/editor.zig:101, and panels.zig's own comment "Save is plan 5's plain save... Plan 6 replaces it with the spec's safe save", Sources/editor/app/panels.zig:12-13].
- Recommended recipe (portable, `std.fs`-only, matches the spec's "write a temporary file and swap it in"): write to `<path>.tmp-<pid or random>` in the same directory (same-volume rename is atomic on both platforms), then `std.fs.rename` it over `path`. On Windows, a plain rename-over-existing-file historically needs `MoveFileEx(..., MOVEFILE_REPLACE_EXISTING)`; verify what `std.fs.Dir.rename`/`renameAbs` actually does on Windows in the Zig version this repo targets before assuming POSIX-style semantics — this is a concrete thing to spike, not to assume. [ASSUMED — Zig's std.fs Windows rename semantics not verified in this session; flag as a checkpoint.]
- `.bak`: "once per session at the first write, holding the version from when the map was opened" (D-19) means copy the *pre-open* file (or the snapshot as first read) to `name.bzm.bak` before the first save of the session — a `std.fs.copyFile`/rename-aside of the original bytes, done once, gated by a per-session flag in `Editor`.
- Autosave-into-a-recovery-copy for a never-saved map (D-22) needs a location; nothing in the codebase currently writes anything like this, so the plan should pick a concrete path (e.g. `<UserRoot>/recovery/<generated name>.bzm`) and state it explicitly rather than leaving it to the executor — CONTEXT.md marks this "Claude's Discretion," so the plan is the right place to fix it, not defer it further.

### Editor settings, mods, recent files (D-24..D-28)

- No settings-file precedent exists in this codebase for the editor; the game's own `config.cfg` is written via `OptionSystem`/`SerializeConfig`, an XML-tree format tied to the option-registration system the editor does not have and should not pull in for a handful of scalar settings. Recommend a small, hand-written key=value or minimal-XML file (`mapeditor.cfg`) parsed with `std.fs`+manual parsing, matching the project's general "no new dependency for a small, well-understood format" pattern (`Sources/src/Platform/WheelScroll.h`'s std-only philosophy, and `MapFile`'s own note about being "GFX-free" and independently testable).
- `-mod=Name`/`-mod=None` and `-profile=` parsing exists in the *game*, not in `MapEditor`'s `main.zig`, which today parses only `<map>`, `--check <map> [out.tga]`, `--smoke <map> [out.bzm]` [VERIFIED: Sources/editor/app/main.zig, docstring at top of file + full arg-parsing loop read]. Plan 6 needs to add `-mod=`/`-profile=`-equivalent parsing to the editor itself (for D-26's "and with `-mod=Name` on the command line, like the game").
- Mods are read-only content mounted from `<install>/mods/<Name>/data/*.pak` [VERIFIED: Sources/src/Main/MainLoopCommands.cpp:391-397]. The editor's object palette reload on mod switch (D-26) should call `BkEditorStart`'s mod-loading step again (or an equivalent bridge re-open), the same way the game's `MAIN_COMMAND_CHANGE_MOD` remounts `MOD` and `GENERATED` storages together.

### Object icons/thumbnails (carried)

- `SGDBObjectDesc` has `szKey`, `szPath` ("path to game resources for this object"), `eVisType`, `eGameType` — **no dedicated icon-image field** [VERIFIED: Sources/src/Main/GameDB.h:46-61, full struct read]. `BkEditorCatalogueEntry` (the bridge's current catalogue record) carries only `name[64]` and `game_type` [VERIFIED: Sources/src/EditorBridge/bridge.h:239-240, Sources/src/EditorBridge/catalogue.cpp full read]. There is no shortcut: an "icon" must be a rendered thumbnail of the object's actual mesh/sprite, using `BkEditorGpuDevice`'s exposed device+format (`bridge.h:265-269`) to render off-screen into a small `SDL_GPUTexture`, which ImGui can then bind as a texture ID.
- Scale risk: the catalogue has ~5.5k entries (plan 5 Task 5's own note, carried-list item "`std.sort.insertion` over ~5.5k catalogue entries") — rendering all of them eagerly at startup is very likely too slow for M1. Recommend on-demand/lazy rendering (only the palette rows currently scrolled into view) with a per-session cache, or a lower-effort fallback (one shared placeholder icon per `game_type`) if a lazy renderer proves too large for this plan's budget — this is exactly the kind of thing to raise as an explicit scope decision in the plan rather than an implicit "and also render 5,500 thumbnails" task.

### Sound list and unknown-objects warning (carried)

- The map's sounds live in `CMapInfo::soundsList` (`SMapSoundInfo`), already read/written unchanged by the snapshot/overlay mechanism; the game hands it to `IScene::InitMapSounds` and the MFC editor edits it in its own `MapSoundInfo.cpp` dialog [CITED: spec text + Sources/src/RandomMapGen/MapInfo_Types.h:188 confirms the field's existence and type via the map struct copy-assignment context read]. No bridge read-out of `soundsList` exists yet (only object/diplomacy/terrain accessors do); a new `BkEditorSounds`-shaped call is needed if the panel is to show/edit anything beyond "there is a sound list."
- Unknown-objects warning: the count is already computed and exposed (`BkEditorMapSummary.unknown_object_count`, `bridge.h:55`) [VERIFIED: bridge.h full read]; the carried-list gap is purely a missing app-side dialog after `BkEditorOpenMap` returns a nonzero count — no bridge work needed.

### `BK_EDITOR_AUTO` and shot comparison

- `smoke.zig`'s scripted-event table (`Sources/editor/app/smoke.zig:1-6` doc comment, and the `Input` union it drives) is explicitly the thing plan 5 said `BK_EDITOR_AUTO` generalises; the game's own `BK_AUTO_UI="frame:action,..."` (`Game/GameMain.cpp:1238-1543`) is the model for the environment-variable-driven syntax and the frame/action logging style (`fprintf(stderr, "BK_AUTO_UI: frame %d action %s\n", ...)`).
- Recommend implementing `BK_EDITOR_AUTO` as a thin generalisation of `smoke.zig`'s existing `Input` union and `script` table (parse a compact string into the same union) rather than a new mechanism, and doing shot comparison as a pixel-tolerance TGA diff (this repo already reads/writes TGA by hand in `smoke.zig`'s captured frames and in `tools/zig/*` — no new image library needed). Store reference images under `zig-out/local-test` per the project's "local test artifacts never /tmp" convention (project memory), not committed to git (large binaries) — a documented, regenerable baseline, refreshed by a human when the rendering intentionally changes.

### Full open/save sweep of shipped maps

- `zig build test-map-files-all` **already exists** and already sweeps all 1,755 shipped `.bzm` files plus the `.xml` maps through read-write-compare-for-equivalence, using the data-only map-file tier [VERIFIED: build.zig:5546-5561, full function tail read, and spec's Testing section: "A local step, `zig build test-map-files-all`, sweeps all 1,755 shipped `.bzm` files and the `.xml` maps."]. This materially reduces plan 6's scope for this exit criterion: the tooling is done; the work is running it, fixing whatever it finds broken (if anything), and citing it as the M1 exit-criterion evidence, not building new sweep tooling.

### Packaging and the Windows console subsystem

- `configureMapEditorExecutable` sets `.subsystem = .console` unconditionally on Windows for every `MapEditor`-shaped executable (the real app and the engine test) [VERIFIED: build.zig:5879-5892, full function read]. The carried-list item wants the *packaged* `MapEditor.exe` to run as `.windows` subsystem (no console window on a normal double-click) while keeping `--check`/`--smoke` output visible when run from CI/a terminal.
- This is safe to do: a `.windows`-subsystem process's stdio still flows through inherited/redirected handles when a parent process (a CI runner, a `cmd.exe` with piping, or `std.process.Child`'s stdio capture) explicitly wires them up — Windows only skips allocating a *new* console window for a subsystem-`.windows` process, it does not sever already-inherited stdio. Recommend switching `configureMapEditorExecutable`'s installed-executable path to `.windows` while leaving CI's own test-only invocations working exactly as they do today (their stderr capture does not depend on subsystem type). [ASSUMED — Windows stdio-inheritance behaviour for GUI-subsystem processes under CI's actual invocation method (GitHub Actions runner, PowerShell) is standard Win32 behaviour but was not empirically re-verified in this session; low risk, cheap to spike locally on the Windows runner before committing to it.]

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---|---|---|---|
| Camera zoom math | A new camera-distance-based zoom | `NSceneScreenScale`'s existing global-var-driven rescale, called the way `ApplyZoomStep` does | It is already the exact function the game's renderer, terrain mesh and sprite scaling all key off; a parallel mechanism would visually diverge from "what the player sees" (D-11) by construction. |
| Zoom-pointer-anchoring | New pointer-anchoring math | `CInterfaceMission::ApplyZoomStep`'s before/after `GetPos3` + anchor-shift recipe | Already solved, already handles the terrain-mesh-rebuild edge case (`ITerrain::ResetPosition()`), copy it rather than rediscover its edge cases. |
| Atomic file save | A custom multi-step write-lock scheme | `std.fs` temp-file + rename in `Editor.save()` | Standard OS-level atomicity; Zig's cross-platform `std.fs` already abstracts POSIX rename vs `MoveFileEx`, but its Windows overwrite semantics still need a one-line verification (see Assumptions Log). |
| Image comparison for shot tests | A new image-diff library dependency | Hand-rolled TGA pixel-tolerance diff, matching existing `tools/zig/*` TGA-reading code | This repo already reads/writes TGA by hand for exactly this purpose (`ReadFramePixels`, `SaveFrame`); a new dependency for the same job is unjustified. |
| Settings file parsing | A generic config-file library | Small hand-written parser (few scalar settings) | Matches the project's `WheelScroll.h`/`MapFile` philosophy of "no new dependency for a well-understood, small format"; the game's own `OptionSystem` is tied to a much larger option-registration system not worth pulling in for four settings. |

**Key insight:** almost everything this phase needs *already exists* somewhere in the engine (zoom math, pointer-anchoring, the sweep tool, the unknown-object count, `-mod=`/`-profile=`/`-windowed` CLI parsing) — the actual new work is wiring the bridge to reach those existing mechanisms from a headless-editor startup that skips the UI-screen machinery (`CInterfaceScreenBase`/`CInterfaceMission`) which normally drives them.

## Common Pitfalls

### Pitfall 1: Treating `ICamera::SetPlacement`'s `fDist` as "the zoom"
**What goes wrong:** A bridge zoom implementation that changes `fDist` will produce a camera that is visually similar but numerically disconnected from `NSceneScreenScale::GetMaxZoomSteps`'s bound, so D-11 ("stay within the game's own limits") becomes unverifiable — there is no "game's own limit" for `fDist`, only for `ZoomSteps`.
**Why it happens:** `SetPlacement`'s signature (`vAnchor, fDist, fPitch, fYaw`) looks exactly like where a "zoom" parameter should live, and the spec's own architecture section describes the bridge's camera surface only in terms of `SetPlacement`/`SetAnchor`.
**How to avoid:** Drive `GFX.World.ZoomSteps`/`ZoomFactor`/`BaseSizeX`/`BaseSizeY` and re-derive the projection the way `NSceneScreenScale` and `CInterfaceScreenBase::Step` do; leave `SetPlacement`'s `fDist` untouched.
**Warning signs:** A zoom that "sort of" respects a hand-picked min/max but produces different visible-world dimensions than the equivalent gameplay screen at the same window size.

### Pitfall 2: Assuming `GFX.World.BaseSizeX/Y` are already set when the bridge starts
**What goes wrong:** `NSceneScreenScale::GetMaxZoomSteps`/`GetGameplayScale` both early-return "no zoom" (`fBaseW/H < 1.0f`) unless these globals are nonzero, and they are only published by `CInterfaceScreenBase::Step` for `szInterfaceType == "Mission"` — a code path the editor's bridge startup (no `CMainLoop`, no interface-screen stack) never runs.
**Why it happens:** It is easy to test zoom against a shipped map and see *some* rendering, since the un-zoomed (`fScale <= 1.0f`) path degrades gracefully — the bug only shows up when trying to zoom in and nothing happens.
**How to avoid:** Have the bridge publish `GFX.World.BaseSizeX/Y` itself (mirroring `InterfaceScreenBase.cpp:704-711`) right after `BkEditorResize`/`BkEditorStart` sets the screen size.
**Warning signs:** `NSceneScreenScale::GetMaxZoomSteps` always returns 0 in the engine tier.

### Pitfall 3: Writing the persistent "maps" folder into the generated-data cache tree
**What goes wrong:** `NProfile::Rename`/`Delete` treat the generated-data directory as best-effort/regenerable (`ProfilePaths.h:161-186`), and its own header comment calls it "a cache" the game can always rebuild from a save's seed. A user's actually-saved map living there risks being silently dropped by any future "clear cache"/profile-delete affordance.
**Why it happens:** It's the one directory this research confirms the game already mounts and finds map files in, so it's tempting to reuse it for everything map-related.
**How to avoid:** Use the generated-data root only for the ephemeral test-launch copy (D-01, explicitly temporary already); keep the persistent D-17 maps folder under `Paths::UserRoot()` directly, opened by plain OS path.
**Warning signs:** A saved map disappearing after a profile operation that was only supposed to touch generated/cached data.

### Pitfall 4: Assuming D-17/D-28's "custom-mission list"/"mod's maps folder" describe existing game behaviour
**What goes wrong:** Building UI copy or acceptance criteria around "the map now shows up in the game's custom-missions screen" or "matches the mod's existing maps layout" will not match what the shipped game actually scans (see Summary point 3), and will fail human verification against the real game.
**Why it happens:** The phrasing reads as a description of existing behaviour; it is closer to a *desired* new convention.
**How to avoid:** Confirm with the user (checkpoint) whether D-17/D-28 mean "the editor's own Open/Save convenience" (recommended, matches what's buildable in M1) or "wire into the game's actual custom-missions/mod system" (a materially larger, game-side feature, likely out of M1's scope).
**Warning signs:** A UAT script that says "open the game's Custom Missions menu and see the saved map" — this will fail today regardless of what plan 6 builds, without additional game-side work not currently scoped.

### Pitfall 5: Building the trackpad rotate gesture before Alt+Q/E
**What goes wrong:** Time sunk into manual two-finger angle tracking (no native SDL3 rotate event) before the guaranteed-simple Alt+Q/E path exists risks landing neither.
**Why it happens:** Gesture work is the "interesting" part; keys feel like an afterthought.
**How to avoid:** Build Alt+Q/E first (small diff on existing Q/E handling), then time-box the gesture.
**Warning signs:** No working camera-rotate input path by the time the gesture gets its first commit.

### Pitfall 6: Adding engine-tier CI coverage without checking the Windows job's remaining budget
**What goes wrong:** The MSVC/GPU Windows CI job's own comments show ~62 of its 75-minute budget already consumed by the engine-tier cold build plus the random-missions tiers plus artifact upload; new engine-tier tests for camera zoom/rotate or object-type coverage could push it over.
**Why it happens:** Locally the engine tier feels fast; CI's cold-cache Windows build is the expensive case, and its cost is not obvious from a plan written on macOS.
**How to avoid:** Check current job duration via `gh run view <run-id>` before adding new engine-tier tests, and prefer running new heavy checks locally-only (like the existing `test-map-files-all`) unless CI coverage is essential to the exit criteria.
**Warning signs:** The Windows job starting to time out on unrelated PRs after this phase merges.

## Code Examples

### Test-launch command line (verified achievable with existing flags)
```
Game -profile=MapEditorTest -mod=<Name-or-None> -windowed maps\<name>.xml
```
Sources: `Sources/src/Game/GameMain.cpp:2001-2039` (`-mod=`, `-profile`, `-windowed` parsing), `:1047-1094` (direct map launch via `IsStreamExist`).

### Publishing the zoom-relevant globals from the bridge (pattern to follow, not copy verbatim — `CInterfaceScreenBase.cpp`'s version is entangled with the interface-screen stack)
```cpp
// Pattern from Sources/src/Common/InterfaceScreenBase.cpp:704-711 (verified read)
SetGlobalVar( "GFX.World.BaseSizeX", nWorldBaseX );
SetGlobalVar( "GFX.World.BaseSizeY", nWorldBaseY );
```
The bridge should call the equivalent after `BkEditorResize`, using the same screen-size values it just applied.

### Zoom step application (copy this recipe into the bridge, not `SetPlacement`'s `fDist`)
```cpp
// Source: Sources/src/GameTT/iMissionInternal.cpp:881-905 (verified)
void CInterfaceMission::ApplyZoomStep( int nDelta )
{
    const CTRect<float> rcScreen = pGFX->GetScreenRect();
    CVec2 vCursor = pCursor->GetPos();
    if ( !rcScreen.IsInside( vCursor ) ) vCursor.Set( rcScreen.Width() / 2, rcScreen.Height() / 2 );
    CVec3 vPosOld( 0, 0, 0 ), vPosNew( 0, 0, 0 );
    pScene->GetPos3( &vPosOld, vCursor, true );
    const int nSteps = Clamp( GetGlobalVar( "GFX.World.ZoomSteps", 0 ) + nDelta, 0, NSceneScreenScale::GetMaxZoomSteps( rcScreen ) );
    SetGlobalVar( "GFX.World.ZoomSteps", nSteps );
    pScene->GetPos3( &vPosNew, vCursor, true );
    pCamera->SetAnchor( pCamera->GetAnchor() + ( vPosOld - vPosNew ) );
    if ( ITerrain *pTerrain = pScene->GetTerrain() ) pTerrain->ResetPosition();
}
```

### Generated-data root (where the test-launch temp map and, separately, per-mod caches live)
```cpp
// Source: Sources/src/StreamIO/GeneratedData.h:34-45 (verified)
inline std::string Root( const std::string &szMOD )
{
    const std::string szProfile = GetGlobalVar( "Profile.Name", "" );
    std::string szRoot = ( std::filesystem::path( NProfile::GeneratedDirectory(
        szProfile.empty() ? std::string( "default" ) : szProfile ) ) / ModKey( szMOD ) ).string();
    // ... backslash normalization, trailing separator
    return szRoot + "\\";
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|---|---|---|---|
| SDL2 `SDL_EVENT_MULTIGESTURE` (pinch distance + rotation angle in one event) | SDL3 3.4.0 splits pinch into its own `SDL_EVENT_PINCH_BEGIN/UPDATE/END` with only `scale`, and drops the combined rotation angle | SDL3 3.4.0 (the version this repo vendors) [VERIFIED: vendor/zig-sdl3/src/events.zig doc comment "available since SDL 3.4.0"] | A trackpad-rotate implementation cannot reuse a single gesture event the way an SDL2 port could; it must derive rotation from raw finger tracking. |

**Deprecated/outdated:** none directly relevant found; this is new-ish SDL3 API territory rather than something being phased out.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|---|---|---|
| A1 | `std.process.Child` is the right, portable way to spawn `Game` from the editor on both macOS and Windows, including keeping the editor responsive while it runs (D-03) | Architecture Patterns / Test launch | If Zig's process API has platform quirks (e.g. Windows job-object/console inheritance), the "editor stays open, game is a separate window" requirement (D-03) could subtly break (e.g. a console flash, or the child dying when the parent's window loses focus). Spike this first. |
| A2 | Zig's `std.fs` rename-over-existing-file is atomic and correct on Windows without extra flags in the Zig version this repo targets | Architecture Patterns / Safe save | If it silently fails or requires an explicit "replace" flag, D-19's safe-save could leave a `.tmp` file behind or fail ungracefully on Windows specifically. |
| A3 | Windows GUI-subsystem (`.windows`) executables still deliver stdout/stderr correctly to a parent that redirects/pipes their handles (relevant to the packaging carried-list item) | Architecture Patterns / Packaging | If CI's specific invocation method doesn't inherit handles as assumed, `--check`/`--smoke` output could go dark after the subsystem switch, breaking CI diagnostics silently. |
| A4 | "Stay within the game's own limits" for camera rotation (D-11/D-12) means "free 360° yaw around the game's fixed pitch," since the game itself never varies yaw and so has no yaw *range* to match | Architecture Patterns / Camera rotation | If the user actually meant something narrower (e.g. a small oscillation range, or no rotation limit language applies to rotation at all), the built feature could not match intent; low cost to ask before implementing. |
| A5 | The editor's settings file (`mapeditor.cfg`) should be a small hand-written format rather than reusing `OptionSystem`/`IDataTree` XML machinery | Architecture Patterns / Settings | If a future milestone wants the settings screen to grow substantially, a hand-written format may need a rewrite; low risk for M1's four settings. |
| A6 | The Windows MSVC/GPU CI job's ~62/75-minute observed budget (from its own code comments) still roughly holds today, rather than having been re-measured and found different since those comments were written | Common Pitfalls #6 | If the actual current headroom is larger (or smaller) than the comment describes, the plan's CI-budget caution could be miscalibrated; cheap to re-check with `gh run view` before adding tests. |

**If this table is empty:** N/A — six assumptions recorded above; all are cheap, concrete things to spike or confirm early rather than blockers.

## Open Questions

1. **Does D-17/D-28's "custom-mission list"/"mod's maps folder" mean wiring into existing game menus, or just the editor's own UX?**
   - What we know: no existing game scanner matches the literal description (see Summary point 3 and Pitfall 4).
   - What's unclear: whether the user's intent, once corrected, still wants some game-side visibility for saved maps in a later milestone, or was only ever describing editor UX.
   - Recommendation: raise as a discuss-phase/plan checkpoint before D-17/D-28 are locked into concrete plan tasks; recommend the editor-UX-only reading for M1.

2. **What exactly should "the game's own limits" mean for camera rotation, given the game itself never rotates?**
   - What we know: `fYaw` is a hard constant in every call site found.
   - What's unclear: whether "free 360°" is the right interpretation or whether some narrower behavior was intended.
   - Recommendation: confirm free 360° yaw at fixed pitch with the user (A4).

3. **Is a lazy/on-demand object-icon renderer feasible within this plan's budget, or should M1 ship a per-`game_type` placeholder instead?**
   - What we know: ~5.5k catalogue entries, no existing icon-rendering code to build from, requires a new off-screen-render-to-texture path through `BkEditorGpuDevice`.
   - What's unclear: the actual render cost per thumbnail and whether ImGui's texture lifetime management (created off the main render thread's SDL GPU device) is straightforward inside this engine's existing render-pass structure.
   - Recommendation: spike a single-object thumbnail render early in the plan; descope to placeholders if it proves expensive, since "Object icons" is explicitly Claude's Discretion in CONTEXT.md.

4. **Does `std.process.Child` (or an equivalent) already get used anywhere else in this codebase for spawning a sibling process, that plan 6 could crib from?**
   - What we know: no occurrence found in the areas searched this session.
   - What's unclear: whether this repo's Zig version's process-spawning API has known rough edges already discovered elsewhere in the project that didn't surface in this search.
   - Recommendation: a quick `grep -r "process.Child" Sources/ tools/` at plan-writing time, and a small standalone spike (spawn `Game --check` from a test `MapEditor` binary) before committing to the exact API shape in a plan.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|---|---|---|---|---|
| SDL3 (vendored) | Pinch/finger gesture events, window/input | ✓ | 3.4.0 | — |
| `Game` executable beside `MapEditor` | Test-launch (D-08) | Built by the same `zig build install-map-editor`/`install-game` steps already in `build.zig` | current | — |
| GPU device on the dev machine (macOS arm64, `metal`) | Engine-tier tests, camera/zoom manual verification | ✓ per the spec's device probe (macOS arm64 confirmed `metal`) | — | — |
| Windows MSVC GPU runner in CI | Engine-tier CI coverage of any new camera/zoom bridge calls | ✓ (per `.github/workflows/cross-platform.yml`), but budget-constrained (see Pitfall 6) | `windows-latest`, `direct3d12` | Run new heavy checks locally-only if CI time is tight |
| `gh` CLI (for checking CI run durations) | Verifying the current Windows job's time budget before adding tests | ✓ (used in this research session) | — | — |

**Missing dependencies with no fallback:** none identified.

**Missing dependencies with fallback:** Windows CI time budget is tight, not missing — see Pitfall 6 for the fallback (run heavy new checks locally-only).

## Validation Architecture

### Test Framework
| Property | Value |
|---|---|
| Framework | Zig `std.testing` (core tier) + custom C++ test executables via `zig build` (map-file and engine tiers) + scripted-SDL smoke (`smoke.zig`) for the app tier |
| Config file | none — steps are defined directly in `build.zig` |
| Quick run command | `zig build test` (core + map-file tiers, all six CI targets) |
| Full suite command | `zig build test-map-editor-engine && zig build map-editor-host-check && zig build map-editor-smoke && zig build test-map-files-all` (engine/app tiers + full sweep, macOS arm64 locally) |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|---|---|---|---|---|
| D-01..D-09 | Test-launch spawns `Game`, plays the saved map, returns to editor on quit | engine/app (new) | new `test-map-editor-testlaunch`-shaped step, or extend `smoke.zig` | ❌ Wave 0 |
| D-10, D-11, D-14 | Zoom respects `GetMaxZoomSteps`, is pointer-anchored | engine | extend `editor_bridge_test.cpp` with a zoom-step regression (mirrors the existing terrain-anchor pattern already carried from Task 6) | ❌ Wave 0 |
| D-12, D-13 | Rotation via Alt+Q/E and reset key | core (`view_math.zig` unit) + app smoke | extend `view_math_test`-shaped tests + `smoke.zig` script step | ❌ Wave 0 |
| D-19..D-23 | Safe save temp+rename, `.bak`, autosave interval, recovery copy | core (`editor.zig` unit tests, matches existing `dirty()`/undo tests already in that file) | `zig build test-editor-core` | ❌ Wave 0 (extend existing file) |
| D-24..D-28 | Settings persistence, recent files, mod switch reload | core/app unit + smoke | new settings-parser tests + `panels_logic.zig`-shaped tests | ❌ Wave 0 |
| Full sweep | Every shipped map opens/saves equivalent | map-file (existing) | `zig build test-map-files-all` | ✅ already exists |
| `BK_EDITOR_AUTO` | Scripted run + shot comparison | app | generalize `smoke.zig`'s `Input`/script table | ❌ Wave 0 (extends existing file) |

### Sampling Rate
- **Per task commit:** `zig build test` (core + map-file tiers; fast, all-platform)
- **Per wave merge:** add `zig build test-map-editor-engine` and `zig build map-editor-smoke` locally on macOS arm64 (per the spec's own test-tier table, these are "required locally," not CI-gated)
- **Phase gate:** `zig build test-map-files-all` full sweep green, plus a manual UAT pass of the M1 exit-criteria bullet ("the editor opens a shipped map, paints tiles, places... test-launches... the game plays the saved map")

### Wave 0 Gaps
- [ ] A test-launch harness test (spawn a fake or the real `Game --check`-equivalent path and assert the process starts/exits as expected) — nothing like this exists yet.
- [ ] `editor_bridge_test.cpp` extension for zoom-step bounds and pointer-anchoring.
- [ ] `editor.zig` extension for safe-save temp+rename+`.bak` behavior (unit-testable with a fake filesystem or a real temp dir, matching the file's existing `std.testing` style).
- [ ] A settings-file round-trip test once the format is chosen.
- [ ] `smoke.zig`/`BK_EDITOR_AUTO` generalisation tests.

## Security Domain

This phase has no network, authentication, or multi-user surface — it is a local native desktop tool spawning a sibling local process and reading/writing local files. Most ASVS categories are not applicable; two are, in a narrow, local sense:

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---|---|---|
| V2 Authentication | no | N/A — no auth surface |
| V3 Session Management | no | N/A |
| V4 Access Control | no | N/A — single local user, local files |
| V5 Input Validation | yes (narrow) | The bridge already validates map files as a whole and rejects malformed ones (`BkEditorOpenMap`'s "a broken map is rejected as a whole" contract, `bridge.h:78-84`); extend the same discipline to any new bridge entry points (zoom deltas, mod names) rather than trusting raw values into `SetGlobalVar`/file paths. |
| V6 Cryptography | no | N/A — no crypto surface introduced |

### Known local-process risk

| Pattern | STRIDE | Standard Mitigation |
|---|---|---|
| Test-launch command-line construction from a mod/profile name the user typed or a mod folder name on disk | Tampering (of the spawned command line, if names contain shell-meaningful characters) | Use `std.process.Child`'s argv-array spawning (not a shell string), so no shell interprets the mod/profile name; this is already this project's general pattern (`NStr::SplitStringWithMultipleBrackets` parses argv tokens, never builds a shell string to `system()`). |
| Writing the test-launch map into the generated-data tree under a profile/mod name | Tampering (path traversal via a crafted profile/mod name) | `NProfile::Sanitize`/`ModKey` already strip path-separator and non-alphanumeric characters from profile/mod names before they become directory components (`ProfilePaths.h:33-49`, `GeneratedData.h:23-32`) — reuse these existing sanitizers rather than re-deriving the allowed-character set. |

## Sources

### Primary (HIGH confidence — read directly this session)
- `Sources/src/Game/GameMain.cpp` — command-line parsing (`-mod=`, `-profile`, `-windowed`, `-map`), direct map launch, `BK_AUTO_UI`.
- `Sources/src/StreamIO/GeneratedData.h`, `Sources/src/StreamIO/ProfilePaths.h` — generated-data root, mount, per-profile/mod paths.
- `Sources/src/Platform/Paths.h`, `Sources/src/Platform/Paths.cpp` — `UserRoot`/`BaseRoot`/`CacheRoot` resolution per platform.
- `Sources/src/Scene/Camera.h`, `Sources/src/Scene/SceneScreenScale.h`, `Sources/src/GameTT/iMissionInternal.cpp` — camera placement vs. the real zoom mechanism.
- `Sources/src/Common/InterfaceScreenBase.cpp` — where `GFX.World.BaseSizeX/Y` are actually published.
- `Sources/src/EditorBridge/bridge.h`, `bridge.cpp`, `session.cpp`, `catalogue.cpp` — current bridge ABI surface, save path, catalogue fields.
- `Sources/src/MapFile/MapFile.cpp` — current (non-atomic) map write.
- `Sources/src/GameTT/CustomMission.cpp`, `Sources/src/Main/MainLoopCommands.cpp` — custom-mission list and mod-mount mechanisms.
- `Sources/editor/app/view.zig`, `main.zig`, `panels.zig`, `smoke.zig`, `Sources/editor/core/editor.zig` — existing editor-app/core wiring for input, save, and scripted smoke.
- `Sources/src/Main/GameDB.h` — `SGDBObjectDesc` fields (no icon field).
- `vendor/zig-sdl3/src/events.zig`, `build.zig.zon` — SDL3 3.4.0 pinch event, no rotate gesture.
- `build.zig` — `configureMapEditorExecutable`, `test-map-files-all`, `install-map-editor`.
- `.github/workflows/cross-platform.yml` — CI job timeouts and the Windows job's own budget commentary.
- `docs/superpowers/specs/2026-09-19-portable-map-editor-design.md`, `docs/superpowers/plans/2026-09-24-map-editor-05-editor-app.md` — spec and the carried-list/self-review.

### Secondary (MEDIUM confidence)
- `gh run list` output confirming recent CI run history exists and is queryable for future duration checks.

### Tertiary (LOW confidence / assumptions)
- `std.process.Child` behavior on macOS/Windows for this exact use case (A1).
- `std.fs` rename-over-existing-file semantics on Windows in this repo's Zig version (A2).
- Windows GUI-subsystem stdio inheritance under CI's exact invocation method (A3).

## Metadata

**Confidence breakdown:**
- Test-launch mechanism: HIGH — verified end-to-end from command-line parsing through storage mounting with line numbers.
- Camera zoom/rotation: HIGH on "what the game currently does," MEDIUM on "what the bridge should expose" (design choice, not yet built).
- Save/settings/mods: HIGH on "what exists today" (confirmed non-atomic save, confirmed mod location), LOW-MEDIUM on file format choices (explicitly Claude's Discretion).
- Packaging: MEDIUM — the subsystem-switch fix is well-understood Win32 behavior but not empirically re-verified this session.
- Object icons, sound list, BK_EDITOR_AUTO: MEDIUM — clear starting points, real open scope questions (icon rendering cost, comparison tolerances) left for the plan.

**Research date:** 2026-09-28
**Valid until:** 30 days (stable native codebase; re-verify if `Sources/src/Scene`, `Sources/src/Game/GameMain.cpp`, or the SDL3 vendor version changes materially before planning starts)

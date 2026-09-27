# Phase 3: Map editor plan 6: finish M1 - Pattern Map

**Mapped:** 2026-09-28
**Files analyzed:** 15 (new or modified; several existing files carry more than one new capability)
**Analogs found:** 15 / 15 (all have a same-role, same-tier analog already in this codebase)

All analogs below are confirmed git-tracked (`git ls-files`), not gitignored mirrors.

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `Sources/src/EditorBridge/bridge.h` (new zoom/rotate/icon entries) | middleware (C ABI surface) | request-response | itself, existing `BkEditorSetCamera`/`BkEditorGpuDevice` declarations | exact |
| `Sources/src/EditorBridge/session.cpp` (`SetSessionZoom`/`SetSessionYaw`/icon render helpers) | service (engine-facing) | request-response | `SetSessionCamera` (session.cpp:1311-1334) | exact |
| `Sources/src/EditorBridge/bridge.cpp` (`BkEditorSetZoom`, `BkEditorZoomReset`, `BkEditorSetYaw`, icon entry point) | controller (Guarded C-ABI wrapper) | request-response | `BkEditorSetCamera`/`BkEditorFrame` wrappers (bridge.cpp:641-655), `BkEditorAddObject` (bridge.cpp:280-301) | exact |
| `Sources/src/EditorBridge/catalogue.cpp` (icon render path, if added here) | service | CRUD (read-only catalogue) | `ReadCatalogue` (catalogue.cpp:13-50) | exact |
| `Sources/editor/app/c_bridge.zig` (`setZoom`, `setYaw`, `renderIcon`, direct RealBridge methods) | service (Zig-to-C adapter) | request-response | `setCamera`/`tilesetTiles`/`catalogue` direct methods (c_bridge.zig:213-246) | exact |
| `Sources/editor/app/view.zig` (pinch/finger rotate tracking, Alt+Q/E, Home reset, zoom wheel) | controller (input routing) | event-driven | `handleWheel`/`handleKey` (view.zig:222-255) | exact |
| `Sources/editor/app/view_math.zig` (zoom-step / rotation math, pointer test additions) | utility (pure math) | transform | existing `wheelPan`/`Camera.panScreen` functions in the same file | exact |
| `Sources/editor/core/editor.zig` (`save()` rewritten for atomic temp+rename, `.bak`, autosave, recovery copy) | service (core edit/save logic) | file-I/O | its own `save`/`open`/`dirty` (editor.zig:75-113) | exact |
| new `Sources/editor/core/settings.zig` (or similarly named) — parses/writes `mapeditor.cfg`, recent files, autosave interval | service (settings persistence) | file-I/O | `Document`'s plain-struct-plus-`ArrayListUnmanaged` style (document.zig:1-40); `PathSlot`'s thread-safe pattern (panels_logic.zig:150-211) for any cross-thread bits | role-match |
| new `Sources/editor/app/testlaunch.zig` (or similar) — spawns `Game` beside `MapEditor`, writes temp map, builds argv | service (process spawn) | event-driven | `host.zig`'s startup-sequencing style + `panels_logic.PathSlot`'s cross-thread hand-over pattern (no direct spawn analog exists; closest structural fit is `host.zig`'s ordered startup steps) | role-match (no exact analog — flagged below) |
| `Sources/editor/app/panels.zig` (Test menu item, Mod menu, Settings window, Open Recent, unknown-objects dialog, sound-list panel, resize-follow) | component/panel (ImGui) | request-response | its own `drawMenuBar` (panels.zig:236-260) and `State` struct (panels.zig:55-80) | exact |
| `Sources/editor/app/panels_logic.zig` (recent-files list logic, mod-switch reload trigger, unsaved-changes prompt state machine) | utility (testable panel logic) | transform | `FileActions`/`PathSlot` (panels_logic.zig:150-261) | exact |
| `Sources/editor/app/smoke.zig` (`BK_EDITOR_AUTO` generalisation of the `Input`/`script` table) | test (scripted harness) | event-driven | its own `Input` union and `script` table (smoke.zig:1-60+) | exact |
| `build.zig` (`configureMapEditorExecutable` Windows subsystem switch for the installed exe) | config (build script) | batch | its own `configureMapEditorExecutable` (build.zig:5879-5891) | exact |
| `Sources/editor/core/editor_test.zig` / inline tests in `editor.zig` (safe-save, `.bak`, recovery-copy tests) | test | file-I/O | existing inline tests in `editor.zig` (e.g. "saving marks clean, and undoing past the save makes it dirty again", editor.zig:584-599) | exact |

## Pattern Assignments

### `Sources/src/EditorBridge/bridge.h` + `bridge.cpp` + `session.cpp` — camera zoom and rotation (D-10..D-16)

**Analog:** `BkEditorSetCamera` end-to-end (bridge.h:242-250, bridge.cpp:641-647, session.cpp:1311-1334)

**Bridge header doc-comment convention** (bridge.h:34-46, the style every new entry point must match — explains null/failure semantics up front):
```c
/* Starts the engine on a window the caller owns and keeps alive for the
   session. data_root is the directory holding Data and the shared libraries.

   window must not be null - that is BK_EDITOR_BAD_ARGUMENT, a caller bug.
   BK_EDITOR_NO_DEVICE means a real window on which the renderer would not
   start, which is what a runner without a GPU reports and a test may skip on.
   ...
   On failure *out is set to whatever exists - null if it failed before
   allocating - so the caller can print BkEditorLastMessage(*out)
   unconditionally. */
BkEditorStatus BkEditorStart( void *window, const char *data_root, BkEditorSession **out );
```
New zoom/rotate entry points (`BkEditorSetZoom(session, delta, cursor_sx, cursor_sy)`, `BkEditorResetView(session)`, `BkEditorSetYaw(session, yaw_offset)`) must document units (world vs. screen vs. global-var steps) the same way `BkEditorWorldToMap`'s comment does (bridge.h:313-321).

**Guarded C-ABI wrapper pattern** (bridge.cpp:641-647, and the richer `BkEditorAddObject` at bridge.cpp:280-301):
```cpp
BkEditorStatus BkEditorSetCamera( BkEditorSession *pSession, float wx, float wy )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return SetSessionCamera( pSession, wx, wy ) ? BK_EDITOR_OK : BK_EDITOR_REFUSED;
	} );
}
```
Every new zoom/rotate entry point is `Guarded(pSession, [=]() -> BkEditorStatus { ... })`; `Guarded` itself (bridge.cpp:59-77) is the try/catch boundary — never let a new entry point add its own try/catch, reuse this one.

**Core zoom recipe to copy verbatim into the new `SetSessionZoom` helper** (`Sources/src/GameTT/iMissionInternal.cpp:881-905`, cited directly in RESEARCH.md — copy the pointer-anchoring recipe, not just the idea):
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
Must be preceded by publishing `GFX.World.BaseSizeX/Y` (pattern from `Sources/src/Common/InterfaceScreenBase.cpp:704-711`, since the bridge's headless startup never runs `CInterfaceScreenBase::Step`):
```cpp
SetGlobalVar( "GFX.World.BaseSizeX", nWorldBaseX );
SetGlobalVar( "GFX.World.BaseSizeY", nWorldBaseY );
```
Call this right after `BkEditorResize` applies the new screen size — see Pitfall 2 in RESEARCH.md.

**Existing camera-placement call to extend for yaw** (session.cpp:1311-1334, exact statement to change):
```cpp
bool SetSessionCamera( SEditorSession *pSession, float wx, float wy )
{
	...
	pCamera->SetPlacement( CVec3( wx, wy, 0.0f ), 1024 * 4 + fGameplayCameraHeight, -ToRadian( 90.0f + 30.0f ), ToRadian( 45.0f ) );
	pCamera->Update();
	return true;
}
```
D-12's rotation: replace the hardcoded `ToRadian(45.0f)` with `ToRadian(45.0f) + fYawOffset`, threading a new yaw parameter through (matches RESEARCH.md's "extend the bridge's camera call to take a yaw offset" recommendation).

**Error handling pattern:** `pSession->szMessage = "..."; return false;` — every new session-level helper follows the same convention as `SetSessionCamera`'s `"there is no camera"` message (session.cpp:1317-1320); the `Guarded` wrapper turns any C++ throw into `BK_EDITOR_FAILED` and `"the engine threw"` automatically.

---

### `Sources/editor/app/c_bridge.zig` — Zig-side zoom/rotate/icon adapter methods

**Analog:** `setCamera`/`tilesetTiles`/`catalogue` (c_bridge.zig:213-246) — note these are direct `RealBridge` methods, **not** part of `core.bridge.Bridge`'s vtable, because camera/view state is not an undoable document edit.

```zig
pub fn setCamera(self: *RealBridge, wx: f32, wy: f32) Status {
    return status(c.BkEditorSetCamera(self.session, wx, wy));
}

pub fn screenSize(self: *RealBridge) ?[2]i32 {
    var width: c_int = 0;
    var height: c_int = 0;
    if (c.BkEditorScreenSize(self.session, &width, &height) != c.BK_EDITOR_OK) return null;
    return .{ width, height };
}
```
New `setZoom(self, delta, cursor_sx, cursor_sy) Status`, `resetView(self) Status`, `setYaw(self, yaw_offset) Status` go here in the same style — thin wrappers over the new `BkEditor*` C calls, called directly from `view.zig`, exactly the way `setCamera` already is (view.zig:207,230).

---

### `Sources/editor/app/view.zig` — input routing for zoom/rotate (D-10..D-14)

**Analog:** `handleWheel` and `handleKey` (view.zig:218-255)

**Wheel/pan pattern to extend for Shift+wheel zoom** (view.zig:218-231):
```zig
fn handleWheel(self: *View, real: *RealBridge, wheel: sdl3.c.SDL_MouseWheelEvent) void {
    const pan = view_math.wheelPan(.{ .x = wheel.x, .y = wheel.y, .flipped = wheel.direction == sdl3.c.SDL_MOUSEWHEEL_FLIPPED }, view_math.wheel_sensitivity);
    const before_x = self.camera_x;
    const before_y = self.camera_y;
    var camera: view_math.Camera = .{ .x = self.camera_x, .y = self.camera_y };
    camera.panScreen(pan.right_px, pan.up_px, self.map);
    self.camera_x = camera.x;
    self.camera_y = camera.y;
    if (self.camera_x != before_x or self.camera_y != before_y) _ = real.setCamera(self.camera_x, self.camera_y);
}
```
Branch on `wheel.mod`/an explicit Shift check the same shape as `handleKey`'s `command_or_control` check below, routing to `real.setZoom(...)` instead of `panScreen` when Shift is held (D-10).

**Key dispatch pattern to extend for Alt+Q/E and Home** (view.zig:240-255):
```zig
fn handleKey(self: *View, editor: *Editor, key: sdl3.c.SDL_KeyboardEvent) void {
    const command_or_control = key.mod & (sdl3.c.SDL_KMOD_CTRL | sdl3.c.SDL_KMOD_GUI) != 0;
    switch (key.key) {
        sdl3.c.SDLK_DELETE, sdl3.c.SDLK_BACKSPACE => self.dispatch(editor, .{ .key = .delete }),
        sdl3.c.SDLK_Q => if (!key.repeat) self.dispatch(editor, .{ .key = .rotate_left }),
        sdl3.c.SDLK_E => if (!key.repeat) self.dispatch(editor, .{ .key = .rotate_right }),
        ...
    }
}
```
Add an `SDL_KMOD_ALT` check ahead of the existing `SDLK_Q`/`SDLK_E` arms (mirroring `command_or_control`'s pattern), routing Alt+Q/E to a new camera-yaw call instead of the object-rotate tool event (RESEARCH.md's explicit recommendation, session.cpp:190). Add `SDLK_HOME` for D-13's view reset, calling the new `resetView` bridge method.

**Pinch gesture (new, no exact analog):** `handleMiddleButton`'s press/release state-tracking shape (view.zig:192-199) is the closest existing pattern for "track a gesture's start/end across events" — reuse that shape for `SDL_EVENT_PINCH_BEGIN/UPDATE/END` (scale-only zoom) and for the hand-rolled two-finger rotate (`SDL_EVENT_FINGER_DOWN/MOTION/UP`, tracked manually since SDL3 3.4.0 has no rotate event — RESEARCH.md, State of the Art table).

---

### `Sources/editor/core/editor.zig` — safe save, autosave, `.bak`, recovery copy (D-19..D-23)

**Analog:** its own current `save()` (editor.zig:98-109), which the plan replaces in place:
```zig
/// The new path is copied before the bridge writes: once the file is
/// saved, nothing here may fail and leave the document without a path.
/// Copying first also keeps `path` valid when it is the document's own.
pub fn save(self: *Editor, path: []const u8) EditError!void {
    var new_path: std.ArrayListUnmanaged(u8) = .empty;
    errdefer new_path.deinit(self.allocator);
    try new_path.appendSlice(self.allocator, path);
    try self.noteOutcome(self.bridge.saveMap(path));
    self.document.path.deinit(self.allocator);
    self.document.path = new_path;
    self.history.markClean();
}
```
Keep this function's error-propagation shape (`errdefer` before any partial mutation, `noteOutcome` for status→message, `history.markClean()` only after everything else succeeded) while inserting, ahead of `self.bridge.saveMap(path)`: (1) the once-per-session `.bak` copy (gated by a new bool field on `Editor`, following the same style as `next_gesture`/`selection`'s plain fields, editor.zig:19-26), (2) write-to-temp-then-rename instead of a direct `saveMap(path)` call — note `saveMap`/`BkEditorSaveMap` writes straight to the path handed to it (see `NMapFile::Write`, cited below), so the temp-file indirection must happen at this Zig layer, calling `saveMap(tmp_path)` then `std.fs.rename(tmp_path, path)`.

**What `saveMap`/`BkEditorSaveMap` actually does today (why the atomicity must be added here, not in C++)** — `Sources/src/MapFile/MapFile.cpp:148-196` (`NMapFile::Write`):
```cpp
CPtr<IDataStream> pStream = CreateFileStream( pszPath, STREAM_ACCESS_WRITE );
...
pStream->Flush();
```
No temp file, no backup — confirms editor.zig's `save()` is the only place this needs to change (RESEARCH.md, "Safe save, autosave, recovery copy" section).

**Dirty-tracking analog for the autosave-interval gate** (history.zig:118-122, already exercised by editor.zig's own tests at editor.zig:584-599):
```zig
pub fn markClean(self: *History) void { ... }
pub fn dirty(self: *const History) bool { ... }
```
Autosave (D-21) should poll `editor.dirty()` before writing, exactly as the unsaved-changes prompt (D-23) will.

**Test pattern to extend** (editor.zig:584-599, `test "saving marks clean, and undoing past the save makes it dirty again"`) — new tests for `.bak` creation and temp+rename should follow this file's existing `std.testing` + `core.editor.testFixture` idiom rather than introducing a new test harness.

---

### `Sources/editor/app/panels.zig` + `panels_logic.zig` — Test menu, Mod menu, Settings window, Open Recent, unsaved-changes prompt (D-01..D-09, D-24..D-28)

**Analog:** `drawMenuBar` (panels.zig:236-260) for menu items; `FileActions`/`PathSlot` (panels_logic.zig:150-261) for the request/deliver/take state-machine shape any new cross-thread or deferred action (Test-in-game launch, mod switch, settings save) should copy.

**Menu bar pattern** (panels.zig:236-260):
```zig
fn drawMenuBar(state: *State) f32 {
    if (!ig.igBeginMainMenuBar()) return 0;
    ...
    if (ig.igBeginMenu("File")) {
        if (ig.igMenuItemEx("Open...", null, false, true)) state.actions.open_requested = true;
        if (ig.igMenuItemEx("Save", null, false, map_open)) state.actions.save_requested = true;
        if (ig.igMenuItemEx("Save As...", null, false, map_open)) state.actions.save_as_requested = true;
        ...
        if (ig.igMenuItemEx("Quit", null, false, true)) state.actions.quit_requested = true;
        ig.igEndMenu();
    }
    ...
}
```
New "Test", "File → Mod", "File → Open Recent", "Edit/File → Settings..." items are added the same way: a bool flag on a new `actions`-shaped struct (or an extension of `FileActions`), read once per frame by an equivalent of `next()`.

**Request/deliver/take state machine to copy for Test-in-game and for the mod-switch confirmation** (panels_logic.zig:150-211, `PathSlot`, and 216-260, `FileActions.next`):
```zig
pub const PathSlot = struct {
    state: std.atomic.Value(u8) = .init(@intFromEnum(State.idle)),
    ...
    pub fn request(self: *PathSlot, kind: DialogKind) bool { ... }
    pub fn waiting(self: *const PathSlot) bool { ... }
    pub fn deliver(self: *PathSlot, path: ?[]const u8) void { ... }
    pub fn take(self: *PathSlot) ?Result { ... }
};
```
The comment's own reasoning ("SDL may call the callback on another thread ... one dialog at a time") is exactly the shape a Test-launch "child process running / restart? / keep?" state (D-06) needs, and the shape a Settings/Mod dialog's "unsaved changes — Save / Don't save / Cancel" (D-23) prompt needs: request → wait → deliver from whichever thread finishes → take once per frame on the main thread.

**`FileActions.next()`'s exhaustive one-shot dispatch pattern** (panels_logic.zig:238-260) is the template for a new `next()`-shaped method on any Test-launch/Mod/Settings action struct — check the dialog/child-process slot first, then flags in priority order, returning `.none` when nothing is pending.

---

### `Sources/editor/app/smoke.zig` — `BK_EDITOR_AUTO` (carried item)

**Analog:** its own `Input` union and doc comment (smoke.zig:1-60):
```zig
pub const Input = union(enum) {
    key: Key,
    press: Pos,
    drag: Pos,
    release: Pos,
    save_as,
    open_saved,
    ...
};
```
`BK_EDITOR_AUTO`'s environment-variable syntax should parse into this same union (plan 5's own note, carried forward) rather than inventing a second representation; model the frame/action logging on the game's own `BK_AUTO_UI` (`Sources/src/Game/GameMain.cpp:1238-1543`, `fprintf(stderr, "BK_AUTO_UI: frame %d action %s\n", ...)`).

---

### `build.zig` — Windows console subsystem for the packaged `MapEditor.exe`

**Analog:** `configureMapEditorExecutable`'s own current body (build.zig:5879-5891):
```zig
fn configureMapEditorExecutable(exe: *std.Build.Step.Compile, target: std.Build.ResolvedTarget) void {
    if (target.result.os.tag == .windows) {
        exe.subsystem = .console;
        exe.entry = .{ .symbol_name = "mainCRTStartup" };
    }
    if (target.result.os.tag == .macos) exe.rdynamic = true;
    if (target.result.os.tag == .macos) exe.root_module.addRPathSpecial("@executable_path");
}
```
The carried-list item wants the *installed* executable's subsystem to be `.windows` while `--check`/`--smoke` test invocations keep console output. Since this one function configures every `MapEditor`-shaped executable (the real app and the engine test, build.zig:5759/5803), the plan must either add a parameter distinguishing "packaged install" from "test executable" or move the subsystem choice to each call site — do not change the shared function's unconditional `.console` without checking both callers (build.zig:5759, 5803).

## Shared Patterns

### Guarded C-ABI boundary
**Source:** `Sources/src/EditorBridge/bridge.cpp:59-77`
**Apply to:** every new `BkEditor*` entry point (zoom, rotate, view reset, icon render)
```cpp
template<class F>
BkEditorStatus Guarded( BkEditorSession *pSession, F body )
{
	if ( pSession == 0 )
		return BK_EDITOR_NO_SESSION;
	try
	{
		pSession->szMessage.clear();
		return body();
	}
	catch ( ... )
	{
		pSession->szMessage = "the engine threw";
		return BK_EDITOR_FAILED;
	}
}
```

### Status → EditError translation
**Source:** `Sources/editor/core/bridge.zig:22-28`
**Apply to:** any new core-level call that reaches the bridge through `Editor` (not the direct `RealBridge` camera/zoom calls, which stay ungoverned by `EditError` the way `setCamera` already does)
```zig
pub fn check(status: Status) EditError!void {
    return switch (status) {
        .ok => {},
        .refused => error.Refused,
        else => error.Failed,
    };
}
```

### Request/deliver/take cross-thread hand-over
**Source:** `Sources/editor/app/panels_logic.zig:150-211` (`PathSlot`)
**Apply to:** Test-in-game process lifecycle (D-06's restart/keep prompt), any settings/mod dialog that must survive a quit-while-open situation
Key structural point from the file's own comment: "the state is the only thing both threads touch; the buffer is written before it is published (release) and read after it is seen (acquire)" — reuse `std.atomic.Value(u8)` with `.acquire`/`.release`/`.monotonic` ordering exactly as `PathSlot` does, rather than a mutex, to match the rest of the app's concurrency style.

### Path-sanitizing before it becomes a directory component
**Source:** `Sources/src/StreamIO/GeneratedData.h:24-35` (`ModKey`)
```cpp
inline std::string ModKey( const std::string &szMOD )
{
	std::string szKey;
	for ( std::string::size_type i = 0; i < szMOD.size(); ++i )
	{
		const unsigned char c = szMOD[i];
		if ( c == '\\' || c == '/' ) continue;
		szKey += ( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) || c == '-' || c == '_' || c == '.' ) ? char( c ) : '_';
	}
	return ( szKey.empty() || szKey == "." || szKey == ".." ) ? std::string( "base" ) : szKey;
}
```
**Apply to:** any new code building a directory path from a mod name, profile name, or map name the user typed (test-launch's temp map path, `mods/<Name>/maps`, the recovery-copy filename) — reuse this sanitizer or its exact character-allow-list rather than re-deriving one (Security Domain section of RESEARCH.md flags this explicitly).

### Generated-data root and mount, as the model for (but not the location of) the test-launch temp map
**Source:** `Sources/src/StreamIO/GeneratedData.h:39-49, 77-89`
```cpp
inline std::string Root( const std::string &szMOD )
{
	const std::string szProfile = GetGlobalVar( "Profile.Name", "" );
	std::string szRoot = ( std::filesystem::path( NProfile::GeneratedDirectory(
	    szProfile.empty() ? std::string( "default" ) : szProfile ) ) / ModKey( szMOD ) ).string();
	...
	return szRoot + "\\";
}
```
**Apply to:** the test-launch writer, which must compute `<generated root>\maps\<name>.xml`/`.bzm` in exactly this shape so the direct-map-launch branch (`GameMain.cpp:1047-1080`) finds it via `IsStreamExist`. Do **not** reuse this root for D-17's persistent user-maps folder — see Pitfall 3 in RESEARCH.md; keep that under `Paths::UserRoot()` directly.

### Non-atomic engine save (what NOT to build on top of unchanged)
**Source:** `Sources/src/MapFile/MapFile.cpp:148-196`
**Apply to:** any code path that calls `saveMap`/`BkEditorSaveMap` directly — it still writes straight into the destination file with no temp/backup; the safe-save contract (D-19) must be implemented one layer up, in `Editor.save()` (see Pattern Assignments above), not by changing this C++ function.

## No Analog Found

| File | Role | Data Flow | Reason |
|---|---|---|---|
| `Sources/editor/app/testlaunch.zig` (process spawn of `Game`) | service | event-driven | No code in this repo currently spawns a sibling process (`std.process.Child` is unused anywhere in `Sources/` or `tools/` per RESEARCH.md's Open Question 4 and Assumption A1); nearest structural fit is `host.zig`'s ordered-startup-steps style plus `panels_logic.PathSlot`'s cross-thread hand-over for reporting the child's exit back to the main loop, but the spawn call itself is new. **Spike first** (RESEARCH.md's own recommendation): a standalone `Game --check` spawn from a throwaway test binary before committing to the exact API shape in a plan task. |
| Object-icon off-screen render path (wherever it lands — likely `session.cpp`/`catalogue.cpp` plus `panels.zig`'s ImGui texture binding) | service | streaming (per-frame GPU render-to-texture) | `SGDBObjectDesc` and `BkEditorCatalogueEntry` carry no icon field today (`Sources/src/Main/GameDB.h:46-61`, `bridge.h:239-240` — confirmed by full-file read); no existing code in this repo renders an object to an off-screen texture and hands it to ImGui. `BkEditorGpuDevice` (bridge.h:265-269) exposes the raw device+format but nothing yet uses it for a render pass outside the main frame. This is explicitly flagged as a spike risk in RESEARCH.md (Open Question 3): budget a single-thumbnail spike early, fall back to a per-`game_type` placeholder if the render path proves too large for this plan. |
| Settings file format (`mapeditor.cfg`) | config/service | file-I/O | No settings-file precedent exists for the editor; the game's own `config.cfg`/`OptionSystem` is tied to a much larger option-registration system RESEARCH.md explicitly recommends not pulling in. Use a small hand-written key=value parser with `std.fs`, following the project's general "no new dependency for a small, well-understood format" philosophy (cited against `Platform/WheelScroll.h` and `MapFile`'s own std-only design) — there's no existing *file format* to copy, only a *philosophy* to match. |
| Sound-list bridge read-out (`BkEditorSounds`-shaped call) | service | CRUD | `CMapInfo::soundsList` exists and survives the snapshot/overlay unchanged, but no bridge accessor reads it out yet (only object/diplomacy/terrain accessors exist). Model any new accessor on `ReadCatalogue`'s two-pass sizing convention (catalogue.cpp:13-50: ask for the count with a null/zero-capacity buffer, then fill), since it is the closest existing "read a list out through the ABI" pattern. |

## Metadata

**Analog search scope:** `Sources/editor/app/`, `Sources/editor/core/`, `Sources/src/EditorBridge/`, `Sources/src/StreamIO/`, `Sources/src/MapFile/`, `Sources/src/GameTT/` (camera/zoom only), `Sources/src/Game/GameMain.cpp` (CLI parsing only), `build.zig` (`configureMapEditorExecutable` and the two `MapEditor`-shaped executable definitions).
**Files scanned:** ~20 read directly (with line-ranged reads on the two largest, `session.cpp` at 1389 lines and `GameMain.cpp`, to avoid loading whole files); counts and line numbers throughout are from this session's direct reads, consistent with RESEARCH.md's own citations.
**Pattern extraction date:** 2026-09-28

---

## PATTERN MAPPING COMPLETE

**Phase:** 3 - map-editor-plan-6-finish-m1
**Files classified:** 15
**Analogs found:** 15 / 15 (12 exact/role-match with concrete excerpts; 3 areas have no in-repo precedent and are flagged as spikes, not blockers)

### Coverage
- Files with exact analog: 12
- Files with role-match analog: 3 (settings persistence, test-launch spawn service, and the icon-render/sound-list bridge read-outs — each has a structurally close pattern to copy the *shape* of, even without an identical existing feature)
- Files with no analog: 3 (listed above, each with a named fallback/spike recommendation already in RESEARCH.md)

### Key Patterns Identified
- Every new `BkEditor*` C-ABI entry point must go through the existing `Guarded()` template (bridge.cpp:59-77) — no new entry point should add its own try/catch.
- Camera/zoom/rotate calls bypass the core's `Bridge` vtable entirely and are called directly as `RealBridge` methods from `view.zig` (the way `setCamera`/`screenSize`/`catalogue` already are) — they are view state, not undoable document edits.
- Cross-thread or deferred UI actions (file dialogs today; Test-launch process lifecycle and Settings/Mod dialogs in this plan) all reuse `PathSlot`'s request→wait→deliver→take atomic state machine rather than a mutex.
- The engine's real zoom is `NSceneScreenScale`'s global-var-driven rescale (`GFX.World.ZoomSteps`/`BaseSizeX/Y`), not `ICamera::SetPlacement`'s distance argument — the bridge must publish `BaseSizeX/Y` itself and copy `CInterfaceMission::ApplyZoomStep`'s pointer-anchoring recipe verbatim.
- Safe save (D-19) belongs entirely in `Editor.save()` (editor.zig core, Zig `std.fs`); `NMapFile::Write` (the C++ side) stays a direct, non-atomic writer and must not be changed.

### File Created
`/Users/johannes/Projects/src/Blitzkrieg/.worktrees/map-editor-6/.planning/phases/03-map-editor-plan-6-finish-m1/03-PATTERNS.md`

### Ready for Planning
Pattern mapping complete. Planner can now reference analog patterns in PLAN.md files.

# Map Editor Plan 5: The Editor App Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A `MapEditor` executable for macOS arm64 and Windows x64 (MSVC) that opens a shipped map in a window drawn by the engine, puts ImGui panels over it, and edits the map through the plan 4 core — paint, place, select, move, rotate, delete, players and diplomacy, undo and redo — and saves it.

**Architecture:** A Zig executable (`Sources/editor/app`) hosts everything in one process: SDL3 owns the window and events, the C++ engine bridge (plan 3-4) starts the engine on that window, and the Zig editor core (plan 4) turns input into commands. The core reaches the real engine through a new Zig adapter over `bridge.h` (`Sources/editor/app/c_bridge.zig`) that implements the core's `Bridge` vtable. ImGui draws into the engine's own frame through a new bridge call that forwards the GFXGPU overlay hook of plan 1 to the engine's renderer, which today nothing can reach. The window's size is the engine's screen size, one to one, so a mouse position is a screen position.

**Tech Stack:** Zig 0.16, SDL3 (`vendor/zig-sdl3`), Dear ImGui through dcimgui (`Sources/editor/imgui`, `vendor/dcimgui`), the C++ engine bridge and engine statics, GFXGPU (Metal on macOS, Direct3D 12 on Windows).

**Spec:** `docs/superpowers/specs/2026-09-19-portable-map-editor-design.md` (sections "Editor app", "Drawing ImGui on top of the engine", "Data flow → Startup, Open, Edit, Pick", "Errors → Startup, Open, Edit", "Build and packaging", "Testing → Editor app").

**Changed from the spec, by decision on 2026-09-24:** the app is first-class on **macOS arm64 and Windows x64 (MSVC)**, not macOS only. The other four CI targets keep building the core and running its tier; they do not build the app.

## The M1 plan series

1. **Overlay spike** (landed): GFXGPU overlay hook, frame capture, vendored ImGui.
2. **Map files** (landed): `MapFile`, data-only startup, comparator, overlay, terrain function.
3. **Engine bridge** (landed): `InitializeWithWindow`, `EditorBridge`, the engine tier on both GPU runners.
4. **Editor core** (landed, merged to `main` as `38ad2bc8c`): read-back, exact undo, picking; the Zig document, commands, history, tools, fake bridge.
5. **Editor app** (this plan): a host that starts the engine with ImGui over it, the adapter to the real bridge, the map view and its input, the panels, `install-map-editor`, an app smoke test on both GPU runners.
6. **Files and launch:** file dialogs with the spec's safe save, the unsaved-changes prompt, recent files and settings, autosave and restore, test-launch into the game, `BK_EDITOR_AUTO`, the "game reads it" tier, packaging.

Plan 5 ends with an editor you can use from a map path on the command line and from File → Open/Save through the OS dialog; plan 6 makes it safe to rely on.

## Global Constraints

- All work happens on branch `feat/portable-map-editor` in the worktree `.worktrees/map-editor`. Never commit in the main checkout. **Never use `git stash` in any form** — the stash stack is shared with other sessions' worktrees.
- Every commit message ends with the line `Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>`.
- Never run `zig fmt` on `build.zig`; edit it by hand. **Run `zig test tools/zig/build_hermeticity_test.zig` after any `build.zig` change.**
- Test artifacts go under `zig-out/local-test/`, never `/tmp`.
- Build on macOS with `-Dtarget=aarch64-macos`; the Windows CI job passes `-Dtarget=x86_64-windows-msvc` and the four `-Dmsvc-*`/`-Dwindows-sdk-*` flags. Thousands of libc header errors on macOS mean the SDK lookup is broken, not `build.zig`.
- **An assert is not a guard, and Windows-MSVC CI is the only place asserts run.** Debug builds on Windows define `_DEBUG` and `_DO_ASSERT_SLOW`; the portable builds define neither. Guard with an `if`. Any new host `main` on Windows routes CRT asserts and aborts to stderr (`_set_error_mode`, `_set_abort_behavior`, `_CrtSetReportMode`/`_CrtSetReportFile` for `_CRT_ASSERT` and `_CRT_ERROR`) exactly as `tools/zig/editor_bridge_test.cpp:1184-1191` does, or a failed assert hangs CI behind a message box.
- **Cross-module `dynamic_cast` fails on macOS.** Reach an interface's sibling through an in-module accessor (`ITerrain::GetEditor` is the model).
- No C++ exception crosses the C ABI: every bridge entry point goes through `Guarded` (`bridge.cpp:37`).
- The core (`Sources/editor/core`) stays std-only and keeps running on all six targets; the app is where SDL, ImGui and the C ABI live.
- The engine's roots on macOS come from the **current working directory** (`Platform/Paths.cpp:36-41`, the `/proc/self/exe` fallback); on Windows from `SDL_GetBasePath()`. The app runs with its working directory set to the installation it was installed into, like `editor-bridge-test` (`build.zig` `addEditorBridgeTest`).
- The window is created **without** `SDL_WINDOW_HIGH_PIXEL_DENSITY`, like the game's (`Platform/SDLApplication.cpp:172-182`), so a window point is a screen pixel and a mouse position is a screen position with no scale. HiDPI is out of M1.

---

## Decisions this plan takes

**The app is a Zig executable, as the spec says, and Task 2 proves it can be.** No Zig executable here has yet linked the engine's C++ statics — built with `-D_DEBUG -D_DLL` against the debug CRT on Windows — together with ImGui, which `addEditorImgui` builds ReleaseFast and leaves to the consumer's CRT. Task 2 is a host that does exactly that and nothing more, measured on both OSes in CI before anything is built on it. If the CRTs cannot be reconciled, the fallback named there is to build ImGui with the engine's flags for the app, not to rewrite the app in C++.

**ImGui reaches the engine's renderer through the bridge.** The renderer is `GraphicsEngineGpu::renderer_` inside the GFXGPU module; the app must not create a second renderer (the spike did) and must not link `gfxgpu` into itself. `IGFX` gains two virtuals appended at its end — `SetOverlay( callback, user )` and `GetGpuDevice( &device, &format )` — implemented by `GraphicsEngineGpu` over `api_.set_overlay` and `api_.get_gpu_device`, and the bridge forwards them as `BkEditorSetOverlay` and `BkEditorGpuDevice`. The overlay then runs inside the engine's own `Flip`, after the scene, before present: what plan 1 measured.

**The screen is the window.** `BkEditorStart` sets the mode to the window's current size instead of `SetMode( 0, ... )`, which took the desktop size and resized the editor's window (plan 4's carried item). `BkEditorResize( w, h )` re-runs `SetMode` and the projection when the window changes. With no high pixel density and no present offset, mouse coordinates need no conversion (`GraphicsEngineGpu.cpp:975-1027` becomes the identity) — Task 1's test measures that it is.

**The adapter lives in the app, not the core.** `c_bridge.zig` `@cImport`s `bridge.h`, so it cannot be in the std-only core. It implements the core's `Bridge` vtable and adds the calls the app needs that the core does not: start, stop, camera, frame, catalogue, resize, overlay, device, and the two engine-tier checks.

**Camera: scroll only in M1.** The spec lists scroll, rotate and zoom. `BkEditorSetCamera` places the camera; rotation and zoom need engine support nobody has measured. Task 4 implements scrolling (keys, screen edge, middle drag) and records whether `ICamera` offers rotate and zoom; they join plan 6 or M2 on that evidence rather than on a guess.

**Object palette without icons.** The spec's palette has icons; the catalogue has names and game types. Icons need textures loaded from the object database and drawn by ImGui through the engine's device, which is its own piece of work. Plan 5 filters by name and groups by game type; icons go to plan 6.

---

## File Structure

| File | Responsibility |
|---|---|
| `Sources/src/GFX/GFX.h` | `IGFX::SetOverlay`, `IGFX::GetGpuDevice`, appended. |
| `Sources/src/GFXGPU/GraphicsEngineGpu.h/.cpp` | Their implementation over `api_`. |
| `Sources/src/GFX/GraphicsEngine.h/.cpp` | The legacy D3D engine's answer: not supported. Only if it is compiled. |
| `Sources/src/EditorBridge/bridge.h/.cpp`, `session.cpp` | `BkEditorSetOverlay`, `BkEditorGpuDevice`, `BkEditorResize`, `BkEditorScreenSize`; `BkEditorStart` sizes the mode from the window. |
| `tools/zig/editor_bridge_test.cpp` | Engine-tier checks for all of the above. |
| `Sources/editor/app/main.zig` | Entry point: arguments, CRT routing, the loop. |
| `Sources/editor/app/host.zig` | SDL window, engine start and stop, ImGui lifecycle, the overlay. |
| `Sources/editor/app/c_bridge.zig` | The adapter: the core's `Bridge` vtable over `bridge.h`, plus the app-only calls. |
| `Sources/editor/app/view.zig` | The map view: camera scrolling, input to tools, hover. |
| `Sources/editor/app/panels.zig` | Menu bar, tool palette, object palette, properties, players and diplomacy, status bar. |
| `Sources/editor/app/c_bridge_test.zig` | The core run against the real bridge: the engine tier's Zig half. |
| `build.zig` | The app executable, its install into the game layout (`install-map-editor`), `test-map-editor-engine`, `map-editor-smoke`. |
| `.github/workflows/cross-platform.yml` | The app's engine test and smoke on `macos-platform` and `windows-platform`. |

---

### Task 1: The bridge hands out the overlay, the device and the screen size

**Files:**
- Modify: `Sources/src/GFX/GFX.h:121` (`interface IGFX`, append at the end)
- Modify: `Sources/src/GFXGPU/GraphicsEngineGpu.h`, `GraphicsEngineGpu.cpp`
- Modify (only if compiled): `Sources/src/GFX/GraphicsEngine.h`, `GraphicsEngine.cpp`
- Modify: `Sources/src/EditorBridge/bridge.h`, `bridge.cpp`
- Test: `tools/zig/editor_bridge_test.cpp`

**Interfaces:**
- Consumes: `GfxGpuApi::set_overlay`, `get_gpu_device` (`GFXGPU/gfxgpu_c.h:154, 202, 204`); `GfxGpuOverlayCallback` = `void (*)( void *user, void *command_buffer, void *target, uint32_t width, uint32_t height )`.
- Produces (GFX.h, appended to `IGFX`):
  ```cpp
  // A callback the renderer runs every frame after the scene and before
  // present, with that frame's command buffer and colour target (SDL GPU
  // objects, as void*). Null removes it. False when the renderer has no such
  // hook (the legacy D3D engine).
  virtual bool STDCALL SetOverlay( void (*pfnOverlay)( void *pUser, void *pCommandBuffer, void *pTarget, unsigned int nWidth, unsigned int nHeight ), void *pUser ) = 0;
  // The SDL GPU device and the colour format the overlay draws in, for a
  // caller that renders into the frame itself (the editor's ImGui).
  virtual bool STDCALL GetGpuDevice( void **ppDevice, unsigned int *pnFormat ) = 0;
  ```
- Produces (bridge.h):
  ```c
  typedef void (*BkEditorOverlay)( void *user, void *command_buffer, void *target, unsigned int width, unsigned int height );
  BkEditorStatus BkEditorSetOverlay( BkEditorSession *session, BkEditorOverlay overlay, void *user );
  BkEditorStatus BkEditorGpuDevice( BkEditorSession *session, void **out_device, unsigned int *out_format );
  BkEditorStatus BkEditorResize( BkEditorSession *session, int width, int height );
  BkEditorStatus BkEditorScreenSize( BkEditorSession *session, int *out_width, int *out_height );
  ```

- [ ] **Step 1: Write the failing tests**

In `editor_bridge_test.cpp`, after `TestCatalogueCameraAndFrame`:

```cpp
// The overlay runs inside the engine's own frame: a callback set through the
// bridge is called once per BkEditorFrame, with a command buffer and a target,
// and not at all once it is removed.
static int g_nOverlayCalls = 0;
static bool g_bOverlayHadTarget = true;
static void CountOverlay( void *pUser, void *pCommandBuffer, void *pTarget, unsigned int nWidth, unsigned int nHeight )
{
	++g_nOverlayCalls;
	if ( pCommandBuffer == 0 || pTarget == 0 || nWidth == 0 || nHeight == 0 || pUser != &g_nOverlayCalls )
		g_bOverlayHadTarget = false;
}

static void TestOverlayDeviceAndSize( BkEditorSession *pSession, SDL_Window *pWindow )
{
	void *pDevice = 0;
	unsigned int nFormat = 0;
	Check( BkEditorGpuDevice( pSession, &pDevice, &nFormat ) == BK_EDITOR_OK && pDevice != 0 && nFormat != 0,
	       "the engine's GPU device and colour format are handed out" );

	g_nOverlayCalls = 0;
	g_bOverlayHadTarget = true;
	Check( BkEditorSetOverlay( pSession, CountOverlay, &g_nOverlayCalls ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	for ( int i = 0; i < 3; ++i )
		BkEditorFrame( pSession );
	Check( g_nOverlayCalls >= 1 && g_bOverlayHadTarget, NStr::Format( "the overlay ran inside the frame (%d calls)", g_nOverlayCalls ).c_str() );
	BkEditorSetOverlay( pSession, 0, 0 );
	const int nCalls = g_nOverlayCalls;
	BkEditorFrame( pSession );
	Check( g_nOverlayCalls == nCalls, "a removed overlay is not called again" );

	// The screen is the window: no desktop-size mode, no scale between a mouse
	// position and a screen position.
	int nWindowW = 0, nWindowH = 0, nScreenW = 0, nScreenH = 0;
	SDL_GetWindowSize( pWindow, &nWindowW, &nWindowH );
	Check( BkEditorScreenSize( pSession, &nScreenW, &nScreenH ) == BK_EDITOR_OK && nScreenW == nWindowW && nScreenH == nWindowH,
	       NStr::Format( "the screen is the window's size (%dx%d against %dx%d)", nScreenW, nScreenH, nWindowW, nWindowH ).c_str() );

	SDL_SetWindowSize( pWindow, 800, 500 );
	SDL_SyncWindow( pWindow );
	Check( BkEditorResize( pSession, 800, 500 ) == BK_EDITOR_OK, BkEditorLastMessage( pSession ) );
	Check( BkEditorScreenSize( pSession, &nScreenW, &nScreenH ) == BK_EDITOR_OK && nScreenW == 800 && nScreenH == 500,
	       NStr::Format( "a resize is the new screen (%dx%d)", nScreenW, nScreenH ).c_str() );
	SDL_GetWindowSize( pWindow, &nWindowW, &nWindowH );
	Check( nWindowW == 800 && nWindowH == 500, NStr::Format( "and the engine leaves the window at that size (%dx%d)", nWindowW, nWindowH ).c_str() );
	// Screen-to-world still composes after a resize: the camera test's check.
	BkEditorSetCamera( pSession, 83 * 32.0f + 16, 36 * 32.0f + 16 );
	BkEditorFrame( pSession );
	float wx = 0, wy = 0;
	int tx = -1, ty = -1;
	Check( BkEditorScreenToWorld( pSession, 400.0f, 250.0f, &wx, &wy ) == BK_EDITOR_OK &&
	       BkEditorWorldToTile( pSession, wx, wy, &tx, &ty ) == BK_EDITOR_OK && tx == 83 && ty == 36,
	       NStr::Format( "after a resize the middle of the screen is still the camera's cell (%d,%d)", tx, ty ).c_str() );
	BkEditorResize( pSession, 640, 480 );
	SDL_SetWindowSize( pWindow, 640, 480 );
}
```

Replace `83 * 32.0f + 16` and `36 * 32.0f + 16` with however `TestCatalogueCameraAndFrame` places the camera on cell 83,36 — reuse its constants. Pass the harness's window into the test (`main` holds `pWindow`), and call it right after `TestCatalogueCameraAndFrame`.

- [ ] **Step 2: Run the engine tier and watch it fail to compile**

Run: `zig build test-editor-bridge -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: compile errors for the four new entry points.

- [ ] **Step 3: `IGFX` and the GPU engine**

Append the two virtuals of the Interfaces block to `interface IGFX` in `GFX.h` — at its end, so no existing slot moves. In `GraphicsEngineGpu`:

```cpp
bool GraphicsEngineGpu::SetOverlay( void (*pfnOverlay)( void*, void*, void*, unsigned int, unsigned int ), void *pUser )
{
	if ( renderer_ == nullptr || api_.set_overlay == nullptr )
		return false;
	return api_.set_overlay( renderer_, reinterpret_cast<GfxGpuOverlayCallback>( pfnOverlay ), pUser ) == GFXGPU_OK;
}

bool GraphicsEngineGpu::GetGpuDevice( void **ppDevice, unsigned int *pnFormat )
{
	if ( renderer_ == nullptr || api_.get_gpu_device == nullptr || ppDevice == nullptr || pnFormat == nullptr )
		return false;
	uint32_t nFormat = 0;
	if ( api_.get_gpu_device( renderer_, ppDevice, &nFormat ) != GFXGPU_OK )
		return false;
	*pnFormat = nFormat;
	return *ppDevice != nullptr;
}
```

Use the success constant `gfxgpu_c.h` actually defines (`GFXGPU_OK` or whatever it is spelled). The two function-pointer types are the same shape; if the compiler rejects the `reinterpret_cast` because `unsigned int` and `uint32_t` differ on a target, declare the `IGFX` parameter with `uint32_t` from `<stdint.h>` instead.

Find every other class that implements `IGFX` (`grep -rn "public IGFX\b" Sources/src` — `GFX/GraphicsEngine.h` is one). If its `.cpp` is compiled by `build.zig` (`grep -n GraphicsEngine.cpp build.zig`), give it the two methods returning `false`. If it is not compiled, still add the two declarations and bodies so the header stays implementable; say which in the report.

- [ ] **Step 4: The bridge entry points**

In `bridge.cpp`, change `StartRenderer`'s `SetMode( 0, 0, 32, -1, GFXFS_WINDOWED, 0 )` to the window's own size, read with `SDL_GetWindowSize( (SDL_Window*)pWindow, &nW, &nH )` (include `<SDL3/SDL.h>`; the engine already builds against SDL3), and move the projection code into a helper both start and resize call:

```cpp
// The projection the game sets after every mode change (Game/GameMain.cpp:795-801).
void SetScreenProjection( IGFX *pGFX )
{
	const RECT rcScreen = pGFX->GetScreenRect();
	SHMatrix matProjection;
	CreateOrthographicProjectionMatrixRH( &matProjection, float( rcScreen.right - rcScreen.left ), float( rcScreen.bottom - rcScreen.top ),
	                                      1, 1024 * 8 + float( rcScreen.bottom - rcScreen.top ) * 2 );
	pGFX->SetCullMode( GFXC_CW );
	pGFX->SetProjectionTransform( matProjection );
	pGFX->EnableLighting( false );
}
```

Replace the comment above the `SetMode` call with one that says what is now true: the mode is the window's size, so the screen is the window and a mouse position is a screen position.

Then the four entry points, after `BkEditorFrame`, each through `Guarded`, following `BkEditorSetCamera`'s shape:

- `BkEditorSetOverlay`: `bEngineStarted` else REFUSED "the engine is not started"; `pGFX->SetOverlay( overlay, user )` false → REFUSED "this renderer has no overlay".
- `BkEditorGpuDevice`: null out-pointers → BAD_ARGUMENT; `GetGpuDevice` false → REFUSED "this renderer has no SDL GPU device".
- `BkEditorResize`: `width <= 0 || height <= 0` → BAD_ARGUMENT; `SetMode( width, height, 32, -1, GFXFS_WINDOWED, 0 )` false → FAILED "IGFX::SetMode failed"; then `SetScreenProjection( pGFX )`.
- `BkEditorScreenSize`: null out-pointers → BAD_ARGUMENT; the width and height of `GetScreenRect()`.

Document each in `bridge.h` in the file's voice. For `BkEditorSetOverlay` say: the callback runs on the thread that calls `BkEditorFrame`, inside `Flip`, after the scene and before present; no render pass is open; it must not call back into the bridge. For `BkEditorResize` say: call it after the window's size changed, with the window's size in points (which is pixels, because the editor's window has no high pixel density).

`SetMode` with an explicit size resizes the window to that size in points (`GraphicsEngineGpu.cpp:538`); with points equal to pixels that is the size the window already has, so the test's "the engine leaves the window at that size" holds. If it does not — the window ends up a different size — stop and report the measured sizes instead of adjusting the test.

- [ ] **Step 5: Run the engine tier**

Run: `zig build test-editor-bridge -Dtarget=aarch64-macos -Dtest-mode=run`
Expected: `editor-bridge: PASS`. The existing tests that print "the screen is WxH" now print the harness window's 640x480, not the desktop's size; if any existing check assumed the desktop size, change it to take the size from `BkEditorScreenSize`.

- [ ] **Step 6: Commit**

```bash
git add Sources/src/GFX Sources/src/GFXGPU Sources/src/EditorBridge tools/zig/editor_bridge_test.cpp
git commit -m "feat(editor): the bridge hands out the engine's overlay and GPU device, and the screen is the window

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: A Zig host that starts the engine with ImGui over it, on both OSes

The riskiest step of M1's app, and so the smallest one: an executable that proves the build and nothing else.

**Files:**
- Create: `Sources/editor/app/main.zig`, `Sources/editor/app/host.zig`
- Modify: `build.zig` (next to `addEditorBridgeTest`, ~5467)
- Modify: `.github/workflows/cross-platform.yml` (`macos-platform`, `windows-platform`)

**Interfaces:**
- Consumes: Task 1's `BkEditorSetOverlay`, `BkEditorGpuDevice`, `BkEditorResize`, `BkEditorScreenSize`; `BkEditorStart`, `BkEditorOpenMap`, `BkEditorFrame`, `BkEditorStop`; `bk_imgui_backend_*` (`Sources/editor/imgui/imgui_backend.h`); `imgui.overlayCallback` (`Sources/editor/imgui/imgui.zig:10`).
- Produces (host.zig):
  ```zig
  pub const Host = struct {
      window: *sdl3.c.SDL_Window,
      session: *c.BkEditorSession,
      pub fn start(options: Options) HostError!Host;   // window, engine, ImGui, overlay
      pub fn stop(self: *Host) void;
      pub fn beginFrame(self: *Host) void;              // ImGui new frame
      pub fn endFrame(self: *Host) HostError!void;      // igRender, then BkEditorFrame (the overlay draws inside it)
      pub fn handleEvent(self: *Host, event: *const sdl3.c.SDL_Event) bool; // true: ImGui took it
  };
  pub const Options = struct { title: [*:0]const u8, width: c_int = 1280, height: c_int = 800, hidden: bool = false, data_root: [*:0]const u8 = "." };
  ```
  `c` is `@cImport` of `bridge.h`, exported from `c_bridge.zig` in Task 3; in this task `host.zig` imports it directly.
- Produces (build.zig): executable `MapEditor` installed into the game stage root; steps `install-map-editor` and `map-editor-host-check`.

- [ ] **Step 1: The host check, written first**

`main.zig` in this task only knows one mode, `--check <map> [<out.rgba>]`: start hidden, open the map, draw frames with an ImGui window of a known colour at a known place, capture one frame, and verify both the map's pixels and the panel's pixels are there — plan 1's spike check, now through the engine's own frame. The capture uses the engine's existing screenshot path: `IGFX::TakeScreenShot` is reachable from the bridge through the frame-saving code Task 7 of plan 4 added to the engine tier (`editor-bridge-objects.tga`); add `BkEditorCaptureFrame( session, const char *path_tga )` to the bridge if that code lives only in the test, as a thin entry point over the same calls, with an engine-tier check that it writes a file of the screen's size. Read the TGA back in Zig (uncompressed 32-bit TGA: 18-byte header, BGRA rows, bottom-up unless bit 5 of byte 17 is set) and check:

- the pixel at the centre of the ImGui window is the window's colour (magenta, `(255, 0, 255)`, within 2 per channel);
- a pixel well outside it is not magenta and not the clear colour — the map is drawn under the panel.

Print `map-editor: host check PASS (<driver>, <w>x<h>)` or a `FAIL:` line naming which pixel was wrong, and exit 0 or 1. The driver name comes from `SDL_GetGPUDeviceDriver` on the device `BkEditorGpuDevice` returns.

`host.zig`:

```zig
//! The editor's process: one SDL window, the engine started on it through
//! the bridge, and ImGui drawing into the engine's own frame. Nothing here
//! edits; the view and the panels (Tasks 4-5) sit on top.
const std = @import("std");
const sdl3 = @import("sdl3");
const imgui = @import("editor_imgui");
pub const c = @cImport(@cInclude("bridge.h"));

pub const HostError = error{ SdlInitFailed, WindowFailed, EngineFailed, NoDevice, ImguiFailed, FrameFailed };

pub const Options = struct {
    title: [*:0]const u8,
    width: c_int = 1280,
    height: c_int = 800,
    hidden: bool = false,
    data_root: [*:0]const u8 = ".",
};

pub const Host = struct {
    window: *sdl3.c.SDL_Window,
    session: *c.BkEditorSession,

    pub fn start(options: Options) HostError!Host {
        if (!sdl3.c.SDL_Init(sdl3.c.SDL_INIT_VIDEO)) return error.SdlInitFailed;
        errdefer sdl3.c.SDL_Quit();
        // No SDL_WINDOW_HIGH_PIXEL_DENSITY: a point is a pixel, so a mouse
        // position is a screen position (see the plan's Decisions).
        var flags: sdl3.c.SDL_WindowFlags = sdl3.c.SDL_WINDOW_RESIZABLE;
        if (options.hidden) flags |= sdl3.c.SDL_WINDOW_HIDDEN;
        const window = sdl3.c.SDL_CreateWindow(options.title, options.width, options.height, flags) orelse return error.WindowFailed;
        errdefer sdl3.c.SDL_DestroyWindow(window);

        var session: ?*c.BkEditorSession = null;
        const started = c.BkEditorStart(window, options.data_root, &session);
        if (started != c.BK_EDITOR_OK) {
            std.debug.print("map-editor: the engine did not start: {s}\n", .{std.mem.span(c.BkEditorLastMessage(session))});
            if (session) |s| _ = c.BkEditorStop(s);
            return if (started == c.BK_EDITOR_NO_DEVICE) error.NoDevice else error.EngineFailed;
        }
        errdefer _ = c.BkEditorStop(session.?);

        var device: ?*anyopaque = null;
        var format: c_uint = 0;
        if (c.BkEditorGpuDevice(session, &device, &format) != c.BK_EDITOR_OK) return error.NoDevice;
        _ = imgui.c.igCreateContext(null);
        errdefer imgui.c.igDestroyContext(null);
        imgui.c.igGetIO().*.IniFilename = null;
        if (!imgui.c.bk_imgui_backend_init(@ptrCast(window), device, format)) return error.ImguiFailed;
        errdefer imgui.c.bk_imgui_backend_shutdown();
        if (c.BkEditorSetOverlay(session, overlay, null) != c.BK_EDITOR_OK) return error.ImguiFailed;
        return .{ .window = window, .session = session.? };
    }

    pub fn stop(self: *Host) void {
        _ = c.BkEditorSetOverlay(self.session, null, null);
        imgui.c.bk_imgui_backend_shutdown();
        imgui.c.igDestroyContext(null);
        _ = c.BkEditorStop(self.session);
        sdl3.c.SDL_DestroyWindow(self.window);
        sdl3.c.SDL_Quit();
        self.* = undefined;
    }

    /// True when ImGui used the event. A window resize is passed to the
    /// engine here, so the screen stays the window.
    pub fn handleEvent(self: *Host, event: *const sdl3.c.SDL_Event) bool {
        if (event.type == sdl3.c.SDL_EVENT_WINDOW_RESIZED) {
            _ = c.BkEditorResize(self.session, event.window.data1, event.window.data2);
        }
        return imgui.c.bk_imgui_backend_process_event(@ptrCast(event));
    }

    pub fn beginFrame(self: *Host) void {
        _ = self;
        imgui.c.bk_imgui_backend_new_frame();
        imgui.c.igNewFrame();
    }

    /// Finishes ImGui's frame and draws the engine's; the overlay puts ImGui's
    /// draw data into it before present. A lost device is a skipped frame,
    /// not an error (BK_EDITOR_REFUSED from BkEditorFrame).
    pub fn endFrame(self: *Host) HostError!void {
        imgui.c.igRender();
        const status = c.BkEditorFrame(self.session);
        if (status != c.BK_EDITOR_OK and status != c.BK_EDITOR_REFUSED) return error.FrameFailed;
    }
};

fn overlay(user: ?*anyopaque, command_buffer: ?*anyopaque, target: ?*anyopaque, width: c_uint, height: c_uint) callconv(.c) void {
    _ = user;
    _ = width;
    _ = height;
    imgui.c.bk_imgui_backend_render(command_buffer, target);
}
```

Take `BkEditorStart`'s real parameter types from the `@cImport` (the window is `void *`, the out-pointer `BkEditorSession **`); adjust the casts to what the compiler says, not the names.

`main.zig` for this task: parse `--check <map> [<out.tga>]` (default output `zig-out/local-test/map-editor-check.tga`), set up CRT routing on Windows (below), start the host hidden, open the map, draw 10 frames with the probe window (copy the spike's `igSetNextWindowPos`/`Size`, `igPushStyleColorImVec4` magenta, `NoDecoration | NoMove | NoSavedSettings` at `(40, 40)` size `(120, 80)`), capture, check, stop.

CRT routing on Windows, as the first thing `main` does — Zig has no `_CrtSetReportMode` binding, so declare the few functions `extern` from the CRT:

```zig
const builtin = @import("builtin");
const windows_crt = if (builtin.os.tag == .windows) struct {
    extern "c" fn _set_error_mode(mode: c_int) c_int;
    extern "c" fn _set_abort_behavior(flags: c_uint, mask: c_uint) c_uint;
    extern "c" fn _CrtSetReportMode(report_type: c_int, mode: c_int) c_int;
    extern "c" fn _CrtSetReportFile(report_type: c_int, file: ?*anyopaque) ?*anyopaque;
} else struct {};

/// A failed assert in a Windows debug build prints and then calls abort(),
/// which the debug CRT reports as a "Debug Error!" message box that nobody
/// on a CI runner can click. Report both to stderr instead
/// (tools/zig/editor_bridge_test.cpp:1184-1191 does the same).
fn routeCrtReportsToStderr() void {
    if (builtin.os.tag != .windows) return;
    const OUT_TO_STDERR = 1;
    const WRITE_ABORT_MSG = 0x1;
    const CALL_REPORTFAULT = 0x2;
    const CRT_ERROR = 1;
    const CRT_ASSERT = 2;
    const CRTDBG_MODE_FILE = 0x1;
    const CRTDBG_FILE_STDERR: ?*anyopaque = @ptrFromInt(@as(usize, @bitCast(@as(isize, -5))));
    _ = windows_crt._set_error_mode(OUT_TO_STDERR);
    _ = windows_crt._set_abort_behavior(0, WRITE_ABORT_MSG | CALL_REPORTFAULT);
    _ = windows_crt._CrtSetReportMode(CRT_ASSERT, CRTDBG_MODE_FILE);
    _ = windows_crt._CrtSetReportFile(CRT_ASSERT, CRTDBG_FILE_STDERR);
    _ = windows_crt._CrtSetReportMode(CRT_ERROR, CRTDBG_MODE_FILE);
    _ = windows_crt._CrtSetReportFile(CRT_ERROR, CRTDBG_FILE_STDERR);
}
```

Check each constant against the MSVC headers (`crtdbg.h`: `_CRT_ERROR` 1, `_CRT_ASSERT` 2, `_CRTDBG_MODE_FILE` 0x1, `_CRTDBG_FILE_STDERR` `((_HFILE)-5)`; `stdlib.h`: `_OUT_TO_STDERR` 1, `_WRITE_ABORT_MSG` 0x1, `_CALL_REPORTFAULT` 0x2). `_CrtSetReportMode`/`_CrtSetReportFile` exist only in the debug CRT; in a release build they are macros that do nothing, so call them only when the build links the debug CRT (`builtin.mode == .Debug` on Windows, matching how `linkMsvcRuntime` picks it).

- [ ] **Step 2: The executable in build.zig**

Add `addMapEditor( b, target, optimize, toolchain, ..., stage_root, install_game_step )` beside `addEditorBridgeTest`, and call it where `addEditorBridgeTest` is called, only when the target is macOS arm64 or Windows x64 MSVC (the app's two platforms). It is the union of two existing recipes:

- from `addEditorBridgeTest` (`build.zig` ~5467-5575): every static library it links (editor_bridge, map_file, main, randommapgen, formats, misc, lualib, zlib, platform_runtime), the SDL import, the full Windows import list (`linkComSupport`, version, winmm, odbc32, odbccp32, shlwapi, advapi32, user32, gdi32, shell32), the include paths, the loader-relative rpath (`@executable_path` on macOS), the install into `stage_root`, the dependency on `install_game_step`;
- from the overlay spike (`build.zig` ~1367-1412): the `sdl3` module import, the `editor_imgui` import (`addEditorImgui`), `addMsvcLibraryPaths`, and the console subsystem on Windows;
- from `addGame` (~3132-3138): `rdynamic = true` on macOS, because engine modules resolve RTTI and the coalesced host globals from the executable (see memory: the host's `g_pGlobalSingleton` copy wins).

Root source `Sources/editor/app/main.zig`; the `bridge.h` include path (`Sources/src/EditorBridge`) added to the module so `@cImport` finds it; name `MapEditor`.

**The CRT is the measured part.** Build it on macOS first; then push and read the Windows job. The engine statics are compiled `-D_DEBUG -D_MT -D_DLL` in Debug and want the debug CRT (`linkMsvcRuntime`); `addEditorImgui` builds ImGui ReleaseFast against whatever the consumer links. The spike used `link_libc = true` on MSVC and no `linkMsvcRuntime`; the bridge test used `linkMsvcRuntime` and no `link_libc`. Start with the bridge test's recipe (`linkMsvcRuntime`, no `link_libc`). If the link fails on `_ITERATOR_DEBUG_LEVEL` or `RuntimeLibrary` mismatches between ImGui's objects and the engine's, build ImGui for the app with the engine's debug flags (a second `addEditorImgui` call parameterised by the flags, or a flag on the existing one) rather than changing the engine's. Record which it was, with the linker's message, in this plan under this step.

Steps:
- `install-map-editor`: installs `MapEditor` into the stage root (depends on `install-game`).
- `map-editor-host-check`: runs `MapEditor --check Data\Maps\Multiplayer\coldwinter.bzm zig-out/local-test/map-editor-check.tga` with the working directory set to the stage root, exactly like `test-editor-bridge`'s run step (`setCwd(stage_root)`), and an absolute output path.

- [ ] **Step 3: Run it on macOS**

Run: `zig build map-editor-host-check -Dtarget=aarch64-macos`
Expected: `map-editor: host check PASS (metal, 1280x800)`. Look at the saved TGA yourself (convert with `sips -s format png`): the map with a magenta box in the top-left.

- [ ] **Step 4: CI on both GPU runners**

Add a step "Map editor host" after "Engine tier" in `macos-platform` and `windows-platform`, running `zig build map-editor-host-check` with that job's exact target, sysroot and MSVC flags (copy them from the "Engine tier" step). Run `zig test tools/zig/build_hermeticity_test.zig`. Commit, push, run the workflow (`gh workflow run "Cross-platform validation" --ref feat/portable-map-editor` — it does not trigger on branch pushes), and read both jobs' output, not only their colour. Expected: `host check PASS (metal, …)` on macOS and `host check PASS (direct3d12, …)` on Windows.

- [ ] **Step 5: Commit**

```bash
git add Sources/editor/app Sources/src/EditorBridge tools/zig/editor_bridge_test.cpp build.zig .github/workflows/cross-platform.yml docs/superpowers/plans/2026-09-24-map-editor-05-editor-app.md
git commit -m "feat(editor): MapEditor starts the engine with ImGui over it, on macOS and Windows

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: The adapter, and the core against the real bridge

**Files:**
- Create: `Sources/editor/app/c_bridge.zig`, `Sources/editor/app/c_bridge_test.zig`
- Modify: `Sources/editor/app/host.zig` (import `c` from `c_bridge.zig`)
- Modify: `build.zig` (the core module for the app's target; step `test-map-editor-engine`)
- Modify: `.github/workflows/cross-platform.yml`

**Interfaces:**
- Consumes: the core's `Bridge`, `Status`, `MapInfo`, `ObjectRecord`, `PaintCell` (`Sources/editor/core/bridge.zig`); `Editor` (`editor.zig`).
- Produces (c_bridge.zig):
  ```zig
  pub const c = @cImport(@cInclude("bridge.h"));
  pub const RealBridge = struct {
      session: *c.BkEditorSession,
      message: [512]u8,
      pub fn init(session: *c.BkEditorSession) RealBridge;
      pub fn bridge(self: *RealBridge) core.bridge.Bridge;
      // app-only, not in the core's vtable:
      pub fn setCamera(self: *RealBridge, wx: f32, wy: f32) core.bridge.Status;
      pub fn screenSize(self: *RealBridge) ?[2]i32;
      pub fn catalogue(self: *RealBridge, allocator: std.mem.Allocator) ![]c.BkEditorCatalogueEntry;
      pub fn engineMatches(self: *RealBridge) core.bridge.Status;   // TerrainMatchesEngine and WorldMatchesMap
  };
  ```
- Produces (build.zig): step `test-map-editor-engine` — `c_bridge_test.zig` as a test executable linked like `MapEditor`, run in the stage root.

- [ ] **Step 1: Write the failing engine test**

`c_bridge_test.zig` drives the core's `Editor` over `RealBridge` through every command, then checks the engine agrees at each step. The engine needs a window, so the test starts a hidden host (Task 2) and skips — printing `map-editor-engine: skipped: no GPU device` and passing — only on `error.NoDevice`, never otherwise:

```zig
test "the core drives the real bridge: every command, undone and redone" {
    var host = Host.start(.{ .title = "map-editor-engine", .hidden = true }) catch |err| switch (err) {
        error.NoDevice => {
            std.debug.print("map-editor-engine: skipped: no GPU device\n", .{});
            return;
        },
        else => return err,
    };
    defer host.stop();
    var real = RealBridge.init(host.session);
    var editor = Editor.init(std.testing.allocator, real.bridge());
    defer editor.deinit();

    try editor.open("Data\\Maps\\Multiplayer\\coldwinter.bzm");
    const objects_at_open = editor.document.objects.items.len;
    try std.testing.expect(objects_at_open > 0);

    // A known object nothing refers to, placed in the engine: the first one
    // whose delete the bridge accepts and whose undo brings it back.
    const first = editor.document.objects.items[0];
    const moved: core.editor.Pose = .{ .x = first.x + 64, .y = first.y, .dir = first.dir, .player = first.player };
    try editor.place(first.link_id, moved, 0);
    try std.testing.expectEqual(core.bridge.Status.ok, real.engineMatches());
    _ = try editor.undo();
    try std.testing.expectEqual(first.x, editor.document.find(first.link_id).?.x);

    const gesture = editor.beginGesture();
    try editor.paint(&.{.{ .x = 20, .y = 20, .tile = 1 }}, gesture);
    try editor.paint(&.{.{ .x = 21, .y = 20, .tile = 1 }}, gesture);
    try std.testing.expectEqual(core.bridge.Status.ok, real.engineMatches());
    _ = try editor.undo();
    try std.testing.expectEqual(core.bridge.Status.ok, real.engineMatches());
    _ = try editor.redo();
    try std.testing.expectEqual(core.bridge.Status.ok, real.engineMatches());

    const added = try editor.addObject(first.nameSlice(), first.x + 96, first.y + 96, 0, 0);
    try std.testing.expectEqual(core.bridge.Status.ok, real.engineMatches());
    _ = try editor.undo();
    try std.testing.expect(editor.document.find(added) == null);
    _ = try editor.redo();
    try std.testing.expect(editor.document.find(added) != null);
    try std.testing.expectEqual(core.bridge.Status.ok, real.engineMatches());

    try editor.delete(added);
    _ = try editor.undo();
    try std.testing.expectEqual(added, editor.document.find(added).?.link_id);
    try std.testing.expectEqual(core.bridge.Status.ok, real.engineMatches());

    try editor.setDiplomacy(1, if (editor.document.diplomacy.items[1] == 0) 1 else 0);
    _ = try editor.undo();

    // Everything undone: back to the map as opened, in the engine too.
    while (try editor.undo()) {}
    try std.testing.expect(!editor.dirty());
    try std.testing.expectEqual(objects_at_open, editor.document.objects.items.len);
    try std.testing.expectEqual(core.bridge.Status.ok, real.engineMatches());
}
```

If `first` turns out to be something the engine refuses to move (a bridge span, an unknown object), pick the first object whose `known` is true and whose kind moves — find it by trying `editor.place` and taking the first that does not return `error.Refused`, printing which it was.

- [ ] **Step 2: Run it and watch it fail to compile**

Run: `zig build test-map-editor-engine -Dtarget=aarch64-macos`
Expected: `RealBridge` undeclared (and the step missing until Step 4).

- [ ] **Step 3: The adapter**

```zig
//! The core's Bridge over the real C ABI (Sources/src/EditorBridge/bridge.h).
//! The core is std-only and runs on every target; this file is where it meets
//! the engine, so it lives in the app. Statuses map one to one, strings are
//! copied into NUL-terminated buffers on the way in, and the bridge's last
//! message is copied out, because the bridge's pointer is valid only until
//! its next call.
const std = @import("std");
const core = @import("editor_core");
pub const c = @cImport(@cInclude("bridge.h"));

const Status = core.bridge.Status;
const Bridge = core.bridge.Bridge;
const MapInfo = core.bridge.MapInfo;
const ObjectRecord = core.bridge.ObjectRecord;
const PaintCell = core.bridge.PaintCell;

comptime {
    // The core's PaintCell is handed to BkEditorPaint as it is.
    std.debug.assert(@sizeOf(PaintCell) == @sizeOf(c.BkEditorPaintCell));
    std.debug.assert(@offsetOf(PaintCell, "tile") == @offsetOf(c.BkEditorPaintCell, "tile"));
}

fn status(value: c.BkEditorStatus) Status {
    return std.meta.intToEnum(Status, value) catch .failed;
}

pub const RealBridge = struct {
    session: *c.BkEditorSession,
    message: [512]u8 = undefined,
    message_len: usize = 0,

    pub fn init(session: *c.BkEditorSession) RealBridge {
        return .{ .session = session };
    }

    pub fn bridge(self: *RealBridge) Bridge {
        return .{ .ptr = self, .vtable = &vtable };
    }

    fn from(ptr: *anyopaque) *RealBridge {
        return @ptrCast(@alignCast(ptr));
    }

    /// A path or name as the bridge wants it: NUL-terminated, and short enough
    /// for the buffer - a longer one is refused rather than cut.
    fn terminated(buffer: []u8, text: []const u8) ?[*:0]const u8 {
        if (text.len >= buffer.len) return null;
        @memcpy(buffer[0..text.len], text);
        buffer[text.len] = 0;
        return @ptrCast(buffer.ptr);
    }

    const vtable: Bridge.VTable = .{
        .lastMessage = lastMessage,
        .openMap = openMap,
        .saveMap = saveMap,
        .objects = objects,
        .diplomacy = diplomacy,
        .addObject = addObject,
        .placeObject = placeObject,
        .deleteObject = deleteObject,
        .restoreObject = restoreObject,
        .setDiplomacy = setDiplomacy,
        .setMapType = setMapType,
        .setAttackingSide = setAttackingSide,
        .paint = paint,
        .undoPaint = undoPaint,
        .redoPaint = redoPaint,
        .screenToWorld = screenToWorld,
        .worldToTile = worldToTile,
        .objectAt = objectAt,
    };

    fn lastMessage(ptr: *anyopaque) []const u8 {
        const self = from(ptr);
        const text = std.mem.span(c.BkEditorLastMessage(self.session));
        const len = @min(text.len, self.message.len);
        @memcpy(self.message[0..len], text[0..len]);
        self.message_len = len;
        return self.message[0..len];
    }

    fn openMap(ptr: *anyopaque, path: []const u8, info: *MapInfo) Status {
        const self = from(ptr);
        var buffer: [std.fs.max_path_bytes + 1]u8 = undefined;
        const z = terminated(&buffer, path) orelse return .bad_argument;
        var summary: c.BkEditorMapSummary = std.mem.zeroes(c.BkEditorMapSummary);
        const result = status(c.BkEditorOpenMap(self.session, z, &summary));
        if (result == .ok) info.* = .{
            .width_tiles = summary.width_tiles,
            .height_tiles = summary.height_tiles,
            .season = summary.season,
            .player_count = summary.player_count,
            .map_type = summary.map_type,
            .attacking_side = summary.attacking_side,
        };
        return result;
    }

    fn saveMap(ptr: *anyopaque, path: []const u8) Status {
        const self = from(ptr);
        var buffer: [std.fs.max_path_bytes + 1]u8 = undefined;
        const z = terminated(&buffer, path) orelse return .bad_argument;
        return status(c.BkEditorSaveMap(self.session, z));
    }

    fn objects(ptr: *anyopaque, out: []ObjectRecord, total: *usize) Status {
        const self = from(ptr);
        var count: c_int = 0;
        const sizing = status(c.BkEditorObjects(self.session, null, 0, &count));
        if (sizing != .ok and sizing != .refused) return sizing;
        total.* = @intCast(count);
        if (out.len < total.*) return .refused;
        if (total.* == 0) return .ok;
        // BkEditorObjects has no offset, so the whole list is read into C
        // records once and converted. The core has already sized `out`; this
        // buffer lives only for the call.
        const records = std.heap.c_allocator.alloc(c.BkEditorObjectRecord, total.*) catch return .failed;
        defer std.heap.c_allocator.free(records);
        var got: c_int = 0;
        const read = status(c.BkEditorObjects(self.session, records.ptr, count, &got));
        if (read != .ok) return read;
        for (records, out[0..records.len]) |record, *object| object.* = toRecord(record);
        return .ok;
    }

    fn toRecord(record: c.BkEditorObjectRecord) ObjectRecord {
        var object: ObjectRecord = .{
            .link_id = record.link_id,
            .x = record.x,
            .y = record.y,
            .dir = record.dir,
            .player = record.player,
            .scenario = record.scenario != 0,
            .known = record.known != 0,
        };
        object.setName(std.mem.sliceTo(&record.name, 0));
        return object;
    }

    fn diplomacy(ptr: *anyopaque, player: i32, value: *i32) Status {
        return status(c.BkEditorDiplomacy(from(ptr).session, player, value));
    }

    fn addObject(ptr: *anyopaque, name: []const u8, x: f32, y: f32, dir: i32, player: i32, link_id: *i32) Status {
        var buffer: [core.bridge.name_capacity]u8 = undefined;
        const z = terminated(&buffer, name) orelse return .bad_argument;
        return status(c.BkEditorAddObject(from(ptr).session, z, x, y, dir, player, link_id));
    }

    fn placeObject(ptr: *anyopaque, link_id: i32, x: f32, y: f32, dir: i32, player: i32) Status {
        return status(c.BkEditorPlaceObject(from(ptr).session, link_id, x, y, dir, player));
    }

    fn deleteObject(ptr: *anyopaque, link_id: i32) Status {
        return status(c.BkEditorDeleteObject(from(ptr).session, link_id));
    }

    fn restoreObject(ptr: *anyopaque, link_id: i32) Status {
        return status(c.BkEditorRestoreObject(from(ptr).session, link_id));
    }

    fn setDiplomacy(ptr: *anyopaque, player: i32, value: i32) Status {
        return status(c.BkEditorSetDiplomacy(from(ptr).session, player, value));
    }

    fn setMapType(ptr: *anyopaque, value: i32) Status {
        return status(c.BkEditorSetMapType(from(ptr).session, value));
    }

    fn setAttackingSide(ptr: *anyopaque, value: i32) Status {
        return status(c.BkEditorSetAttackingSide(from(ptr).session, value));
    }

    fn paint(ptr: *anyopaque, cells: []const PaintCell, token: *i32) Status {
        return status(c.BkEditorPaint(from(ptr).session, @ptrCast(cells.ptr), @intCast(cells.len), token));
    }

    fn undoPaint(ptr: *anyopaque, token: i32) Status {
        return status(c.BkEditorUndoPaint(from(ptr).session, token));
    }

    fn redoPaint(ptr: *anyopaque, token: i32) Status {
        return status(c.BkEditorRedoPaint(from(ptr).session, token));
    }

    fn screenToWorld(ptr: *anyopaque, sx: f32, sy: f32, wx: *f32, wy: *f32) Status {
        return status(c.BkEditorScreenToWorld(from(ptr).session, sx, sy, wx, wy));
    }

    fn worldToTile(ptr: *anyopaque, wx: f32, wy: f32, tx: *i32, ty: *i32) Status {
        return status(c.BkEditorWorldToTile(from(ptr).session, wx, wy, tx, ty));
    }

    fn objectAt(ptr: *anyopaque, sx: f32, sy: f32, link_id: *i32) Status {
        return status(c.BkEditorObjectAt(from(ptr).session, sx, sy, link_id));
    }

    pub fn setCamera(self: *RealBridge, wx: f32, wy: f32) Status {
        return status(c.BkEditorSetCamera(self.session, wx, wy));
    }

    pub fn screenSize(self: *RealBridge) ?[2]i32 {
        var width: c_int = 0;
        var height: c_int = 0;
        if (c.BkEditorScreenSize(self.session, &width, &height) != c.BK_EDITOR_OK) return null;
        return .{ width, height };
    }

    /// The object database, for the palette. Caller frees.
    pub fn catalogue(self: *RealBridge, allocator: std.mem.Allocator) ![]c.BkEditorCatalogueEntry {
        var count: c_int = 0;
        _ = c.BkEditorCatalogue(self.session, null, 0, &count);
        const entries = try allocator.alloc(c.BkEditorCatalogueEntry, @intCast(count));
        errdefer allocator.free(entries);
        if (c.BkEditorCatalogue(self.session, entries.ptr, count, &count) != c.BK_EDITOR_OK) return error.CatalogueFailed;
        return entries;
    }

    /// The engine tier's two agreement checks, for tests.
    pub fn engineMatches(self: *RealBridge) Status {
        const terrain = status(c.BkEditorTerrainMatchesEngine(self.session));
        if (terrain != .ok) return terrain;
        return status(c.BkEditorWorldMatchesMap(self.session));
    }
};
```

In `build.zig`, the app needs the core as a module built for the app's target (the core tier's module is host-only): `b.createModule(.{ .root_source_file = b.path("Sources/editor/core/root.zig"), .target = target, .optimize = optimize })`, imported as `editor_core` by `MapEditor` and the new test. `test-map-editor-engine` is `b.addTest` over `c_bridge_test.zig` with exactly `MapEditor`'s linking (factor `addMapEditor`'s linking into a helper both use), run in the stage root.

- [ ] **Step 4: Run it**

Run: `zig build test-map-editor-engine -Dtarget=aarch64-macos`
Expected: the test passes. Add the step to both GPU jobs after "Map editor host", run CI, and read both logs.

- [ ] **Step 5: Commit**

```bash
git add Sources/editor/app build.zig .github/workflows/cross-platform.yml
git commit -m "feat(editor): the core drives the real bridge through a Zig adapter, checked against the engine

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 4: The map view

**Files:**
- Create: `Sources/editor/app/view.zig`
- Modify: `Sources/editor/app/main.zig` (the interactive loop)

**Interfaces:**
- Consumes: `Host` (Task 2), `RealBridge` (Task 3), the core's `Editor`, `tools.Brush`, `tools.Placer`, `tools.Selector`, `tools.Event`, `tools.Key`.
- Produces (view.zig):
  ```zig
  pub const Tool = enum { select, brush, place };
  pub const View = struct {
      camera_x: f32, camera_y: f32,
      tool: Tool,
      brush: tools.Brush, placer: tools.Placer, selector: tools.Selector,
      hover: ?tools.Pointer,
      pub fn init(allocator: std.mem.Allocator) View;
      pub fn deinit(self: *View, allocator: std.mem.Allocator) void;
      pub fn centreOn(self: *View, real: *RealBridge, info: MapInfo) void;
      /// An SDL event ImGui did not take. Returns the edit's error for the status line.
      pub fn handleEvent(self: *View, editor: *Editor, real: *RealBridge, event: *const sdl3.c.SDL_Event) void;
      /// Per frame: keyboard and edge scrolling, then the camera.
      pub fn update(self: *View, real: *RealBridge, dt_seconds: f32) void;
  };
  ```

- [ ] **Step 1: Tests for what can be tested without a window**

The view's pure parts get unit tests in `view.zig` (they build with the app, run under `test-map-editor-engine`'s linking or a plain `b.addTest` if `view.zig` imports nothing C — keep the pure functions in a separate `view_math.zig` if that is what it takes to test them without the engine):

```zig
test "scroll speed: edge and keys add up, and clamp at the map" {
    var camera: Camera = .{ .x = 100, .y = 100 };
    camera.scroll(.{ .left = true }, 0.5, .{ .width_tiles = 96, .height_tiles = 96 });
    try std.testing.expect(camera.x < 100);
    camera = .{ .x = 0, .y = 0 };
    camera.scroll(.{ .left = true, .up = true }, 10, .{ .width_tiles = 96, .height_tiles = 96 });
    try std.testing.expectEqual(@as(f32, 0), camera.x);
    try std.testing.expectEqual(@as(f32, 0), camera.y);
}

test "a mouse button maps to a tool event, and only the left button edits" {
    try std.testing.expectEqual(EventKind.press, kindOf(sdl_left_down).?);
    try std.testing.expect(kindOf(sdl_right_down) == null);
}
```

Fill in `Camera`, `Scroll` and `kindOf` as the implementation needs; the point is that scrolling limits and the button mapping are pinned without a GPU.

- [ ] **Step 2: The view**

- **Camera.** World units: a tile is 32 (`tile_size` in the fake is the same constant; confirm it against the engine's `SAIConsts::TILE_SIZE` or whatever `WorldToTile` divides by, and name the constant after the engine's). Start centred on the map (`width_tiles * 32 / 2`). Scroll with arrow keys and WASD at 1,000 world units per second, with the mouse within 8 pixels of a window edge, and by dragging with the middle button (the drag moves the camera by the world delta under the cursor, through `resolve`'s `world_x/y`). Clamp to the map. Call `real.setCamera` when it moved. Keys only scroll when ImGui does not want the keyboard (`igGetIO().*.WantCaptureKeyboard`).
- **Rotate and zoom.** Read `Sources/src/Scene/Camera*.h` (`ICamera`) for anything that rotates or zooms the view (the game's own camera). Record what exists in this plan under this step. Do not implement either in this task.
- **Input to tools.** On `SDL_EVENT_MOUSE_BUTTON_DOWN` (left) → `editor.resolve(x, y)` → `tool.handle(.press)`; motion with the left button held → `.drag`; up → `.release`. Keys: Delete/Backspace → `.key = .delete`; `Q`/`E` → rotate left/right; `1`/`2`/`3` choose select/brush/place; Cmd+Z / Ctrl+Z undo; Shift+Cmd+Z / Ctrl+Y redo. `error.Refused` is not an error here: the editor's status line already holds the reason. Any other error from an edit is shown on the status line too, prefixed "failed:".
- **Hover.** Every motion event resolves the pointer and keeps it in `hover` for the status bar and the brush outline (the outline is drawn by ImGui's background draw list at the hovered cells' screen corners — take the corners from the tile's world corners through a world-to-screen conversion if the bridge offers one; if not, skip the outline and record that the bridge needs `BkEditorWorldToScreen`).
- `main.zig` gains the interactive mode: `MapEditor [<map>]` opens a visible window, opens the map if given, runs the loop — poll events (host first, then the view when ImGui did not take it), `view.update`, `host.beginFrame`, panels (Task 5; for now a one-line status window), `host.endFrame` — until the window closes.

- [ ] **Step 3: Try it by hand, and record what you saw**

Run: `zig build install-map-editor -Dtarget=aarch64-macos`, then from the stage root `./MapEditor 'Data\Maps\Multiplayer\coldwinter.bzm'`. Scroll, paint a few cells with `2`, place with `3` (the placer's object is the first catalogue entry of game type unit until the palette exists), select and drag one with `1`, delete it, undo everything. Save one screenshot per tool under `zig-out/local-test/`. Record in the plan what worked and what did not.

- [ ] **Step 4: Commit**

```bash
git add Sources/editor/app docs/superpowers/plans/2026-09-24-map-editor-05-editor-app.md
git commit -m "feat(editor): the map view scrolls, and the mouse and keys drive the tools

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 5: The panels

**Files:**
- Create: `Sources/editor/app/panels.zig`
- Modify: `Sources/editor/app/main.zig`, `view.zig`

**Interfaces:**
- Consumes: `Editor` (document, status, undo/redo, the setters), `View` (tool, brush, placer), `RealBridge.catalogue`.
- Produces (panels.zig): `pub fn draw(state: *State) void` with `State` holding the editor, the view, the catalogue, the palette filter and the file actions requested this frame (`open_requested`, `save_requested`, `save_as_requested`, `quit_requested`).

- [ ] **Step 1: The panels**

Each in its own ImGui window, docked to the window's edges with `igSetNextWindowPos/Size` on first use (`ImGuiCond_FirstUseEver`); the map view is everything the panels do not cover.

- **Menu bar:** File → Open…, Save, Save As…, Quit; Edit → Undo (disabled when `!history.canUndo()`), Redo; Tools → Select, Brush, Place. Open and Save As use SDL's file dialogs (`SDL_ShowOpenFileDialog` / `SDL_ShowSaveFileDialog`, filter `*.bzm;*.xml`); their callbacks arrive asynchronously on the main thread through the event loop — keep the chosen path in `State` and act on it in the next frame. Save writes to the document's path. **This is plan 5's plain save; plan 6 replaces it with the spec's safe save and the unsaved-changes prompt.** The window title shows the map's file name and a `*` when `editor.dirty()`.
- **Tool palette:** three buttons, the active one highlighted; the brush's tile index (0 to the tileset's count — ask the engine for the count if the bridge can say; if not, 0-15 with a note) and radius (0-4).
- **Object palette:** the catalogue, grouped by `game_type` (collapsing headers named by the engine's `SGVOGT_*` names — copy them from `Sources/src/Formats/fmtObject.h` or wherever the enum lives), filtered by a text box (case-insensitive substring). Clicking an entry sets the placer's object and switches to the place tool. No icons (see Decisions).
- **Properties:** for the selected object: name, link ID, position (x, y), direction in degrees (converted from the engine's 65536), player — editable with `igInputFloat`/`igSliderInt`; an edit is one `editor.place` call with `gesture` 0 when the widget is deactivated after an edit (`igIsItemDeactivatedAfterEdit`), so a typed number is one undo step. Unknown and shared-ID objects show "kept as it is" and no editable fields.
- **Players and diplomacy:** one row per player with a combo of side 0, side 1, neutral (`editor.setDiplomacy`); the map type and the attacking side (`setMapType`, `setAttackingSide`).
- **Status bar:** `editor.status()` (the last refusal or failure), the hovered tile and world point, the tool.

- [ ] **Step 2: A panel smoke in the host check**

Extend `--check` so after the pixel checks it also draws one frame of the real panels over the map (with `State` from the opened map) and exits 0 when nothing failed — so CI exercises the panel code on both GPU runners without a person. Keep the magenta probe check as it is.

- [ ] **Step 3: Try it by hand**

As in Task 4, with the panels: open a map through File → Open, place an object from the palette, change its player in Properties, change a side in Players, undo and redo through the menu, save through Save As to `zig-out/local-test/`, reopen the saved file. Record it in the plan.

- [ ] **Step 4: Commit**

```bash
git add Sources/editor/app docs/superpowers/plans/2026-09-24-map-editor-05-editor-app.md
git commit -m "feat(editor): menu, tools, object palette, properties, players and status panels

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 6: Terrain draws everywhere the camera looks

Carried from plan 4: in parts of the map the terrain draws black under objects that do draw (`zig-out/local-test/editor-bridge-objects.tga`, W_BigPoplar on coldwinter). Plan 4's final review traced it outside the bridge: a vertical cut at about x=160 of 1440 points at screen-space clipping in `CTerrain::AddVertices`/`CheckForRect` (`Scene/TerrainInternal.cpp:308-341`) or the rectangle `CTerrain::ExtractVisiblePatches` builds from the screen size (`Scene/TerraDraw.cpp:230-277`); `IGFX::GetViewVolume` is empty in the GPU adapter (`GraphicsEngineGpu.cpp:821`).

**Files:** determined by the diagnosis; `tools/zig/editor_bridge_test.cpp` for the check.

- [ ] **Step 1: Measure first**

With the app of Tasks 2-5 at the same camera anchor as `editor-bridge-objects.tga`, and the game at the same anchor on coldwinter (`BK_AUTO_UI` `camera=XxY`, then `shot` — see `Game/GameMain.cpp:1234-1512`), capture both with `BK_GFX_TRACE` on and compare the terrain draw calls (effects 101, 2 and 100) and their vertex counts. The game is the reference: if the game draws the terrain there, the editor is missing a step the game does (a per-frame terrain update, the camera's view volume, the screen rectangle); if the game draws it black too, the cause is in the engine and the fix is there. Record the measurements in the plan.

- [ ] **Step 2: Make the engine tier catch it**

Add an engine-tier check that a frame at that anchor has terrain: in the captured TGA, the fraction of black pixels in the screen's lower half (below the sky gradient) is under a threshold measured on a frame known to be good. Print the fraction. Write it before the fix so it fails; it is the regression test.

- [ ] **Step 3: Fix, run both tiers, commit**

Fix where the measurement pointed. Run `test-editor-bridge` and `map-editor-host-check`; look at the new frame. Commit with a message naming the cause.

---

### Task 7: Installed next to the game, and smoke-tested on both GPU runners

**Files:**
- Modify: `build.zig`, `.github/workflows/cross-platform.yml`
- Modify: `Sources/editor/app/main.zig` (`--smoke`)

- [ ] **Step 1: `--smoke`**

A scripted run through the real app's loop, hidden, with no person: open the map, select the tool of each kind in turn, feed synthetic SDL events (`SDL_PushEvent` of mouse down/motion/up and key presses at fixed screen positions) that paint, place, select, drag, rotate, delete, undo all of it, and save to `zig-out/local-test/map-editor-smoke.bzm`; reopen the saved map and check the object count equals the original's (everything was undone). Print `map-editor: smoke PASS` and exit 0. This is the automation plan 6's `BK_EDITOR_AUTO` will generalise; keep the event script in one table so it can.

- [ ] **Step 2: CI**

Add a "Map editor smoke" step after "Map editor host" in `macos-platform` and `windows-platform`. `install-map-editor` already installs the executable into the stage root; confirm the packaged runtime list (`build.zig` ~1570-1580, `tools/zig/stage.zig`) does not need `MapEditor` added for the stage to be complete, and note that packaging it is plan 6. The legacy `Editors/MapEditor.exe` (`stage.zig:450-465`) keeps its name and place; the new one sits in the root beside `Game`. Record that the two names coexist until M3 deletes the old one.

- [ ] **Step 3: Run CI and read both jobs**

Expected on both: `host check PASS`, the core-against-engine test passing, `smoke PASS`.

- [ ] **Step 4: Commit**

```bash
git add Sources/editor/app build.zig .github/workflows/cross-platform.yml docs/superpowers/plans/2026-09-24-map-editor-05-editor-app.md
git commit -m "feat(editor): MapEditor installs beside the game and passes a scripted smoke on both GPU runners

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

## Self-review notes

- **Spec coverage, "Editor app":** SDL window, event and frame loop (Tasks 2, 4); ImGui through dcimgui with the SDL3 and SDL-GPU backends (Task 2); panels — menu bar, tool palette, object palette with filter, properties, players and diplomacy, status bar (Task 5; icons deferred, see Decisions); the map view as the window's background, input over panels to ImGui (Task 4). Settings and recent files: plan 6.
- **Spec coverage, "Data flow":** startup opens the window, starts the engine, fills the palette from the catalogue (Tasks 2, 5); open, edit and pick (Tasks 3-5); save as plain save (Task 5), safe save in plan 6; test launch: plan 6.
- **Spec coverage, "Errors":** startup failure names the step and quits (Task 2's `Host.start` errors; the app shows them in a message box via `SDL_ShowSimpleMessageBox` before exiting — add that in Task 4 when the interactive mode lands); open keeps the previous map (plan 4); refused edits on the status bar (Tasks 4-5). Save's temporary-file swap: plan 6.
- **Spec coverage, "Testing → Editor app":** the scripted smoke (Task 7) is the first half; `BK_EDITOR_AUTO` and the shot comparison are plan 6.
- **Carried from plan 4:** the desktop-size mode (Task 1), black terrain (Task 6), diplomacy reaching the view (the app's frame loop calls `BkEditorFrame` every frame; if the view still lags a diplomacy change, `BkEditorFrame` needs a world update — check in Task 5's manual run and fix in the bridge if so), picking the frontmost object (still open; record in Task 5's manual run whether it bites).
- **Spec correction for plan 6:** the spec's test launch says `-mod<dir>`; the game now takes `-mod=Name` (`game_command_line_test.cpp:74-93`). And the game loads a map only from its data storage (`GameMain.cpp:1047-1075`), so test launch needs a storage the game mounts — the profile's generated-data root (`StreamIO/GeneratedData.h:77-88`) is the likely route; plan 6 measures it.

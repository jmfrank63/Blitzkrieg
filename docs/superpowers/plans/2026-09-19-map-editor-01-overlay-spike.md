# Map Editor Plan 1: ImGui Overlay Spike Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prove that Dear ImGui, called from Zig through the `dcimgui` C bindings, draws on top of a frame rendered by the GPU renderer (GFXGPU), in the same frame, and that a test can measure both from one readback.

**Architecture:** GFXGPU gets an overlay callback that runs in `endFrame` after the scene blit and before submit, and receives the frame's command buffer and colour target. A per-frame capture option composes that frame into a readable texture of drawable size instead of the swapchain, then copies it to the swapchain, so a test can read back exactly what was presented. A small C++ shim wraps ImGui's SDL3 and SDL-GPU backends in a C API; a Zig spike program drives GFXGPU through its C ABI, draws an ImGui panel through the overlay, and checks the pixels.

**Tech Stack:** Zig 0.16, SDL3 GPU (via `vendor/zig-sdl3` and the `sdl` package), Dear ImGui 1.92.9b docking (`floooh/dcimgui` v1.92.9b, backends from `ocornut/imgui` v1.92.9b-docking), C++17.

> **Amended 2026-09-21 after review.** The capture texture is created by
> `sdl.createCaptureTexture`, not `createColorTexture`: same two usages
> (`COLOR_TARGET` for the render pass, `SAMPLER` for the blit to the swapchain)
> but stated where the capture path can be read, so they cannot drift with
> another caller's needs. A transfer-source usage was asked for and does not
> exist: SDL 3.4.0 defines seven texture usages (`SDL_gpu.h:906-912`), none of
> them transfer or copy-source, and `SDL_DownloadFromGPUTexture` documents no
> usage requirement. The open item is not a flag but a run, and it is narrower
> than it first looked: on Metal both the visible and the hidden spike pass and
> were re-measured after this change - `driver=metal format=12 size=320x240
> inside=(255,0,255) outside=(0,255,0)`, identical either way. What remains
> unmeasured is Direct3D and Vulkan, because CI runs
> `editor-overlay-spike-build`, which compiles the spike everywhere and runs it
> nowhere.

**Spec:** `docs/superpowers/specs/2026-09-19-portable-map-editor-design.md` (section "Drawing ImGui on top of the engine", and "Testing → Overlay spike").

## The M1 plan series

The spec's M1 is split into plans that each end in working, tested software. This is plan 1. The later ones are written when their predecessor has landed, so they can build on what was learned:

1. **Overlay spike** (this plan): GFXGPU overlay hook, frame capture, vendored ImGui, the spike program.
2. **Map files:** `Formats/MapFile.{h,cpp}`, the data-only startup, the equivalence comparator, the snapshot overlay without the engine, the terrain function, the map-file test tier in CI.
3. **Engine bridge:** `NMain::InitializeWithWindow`, `Sources/src/EditorBridge` C ABI, building the engine state, editing calls, picking, camera, the engine test tier.
4. **Editor core:** Zig document, commands, undo and redo, tools, fake bridge, core tests on all six targets.
5. **Editor app:** window, frame loop, ImGui panels, `BK_EDITOR_AUTO`, save, autosave, test-launch, `install-map-editor`, packaging.

## Global Constraints

- All work happens on branch `feat/portable-map-editor` in the worktree `.worktrees/map-editor`. Never commit in the main checkout.
- Every commit message ends with the line `Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>`.
- Never run `zig fmt` on `build.zig` or gate on `zig fmt --check` for it; edit it by hand.
- Test artifacts (screenshots, readbacks, logs) go under `zig-out/local-test/`, never `/tmp`.
- Build on macOS with `zig build <step> -Dtarget=aarch64-macos`. If the build fails with thousands of libc header errors (FP_ZERO, ldiv_t), the macOS SDK lookup is broken (`xcode-select`), not `build.zig`.
- GFXGPU C ABI rules: entries are only ever **appended** to `Api` (`Sources/src/GFXGPU/abi.zig`) and mirrored in the same order in `GfxGpuApi` (`Sources/src/GFXGPU/gfxgpu_c.h`); `api_base_size` does not change.
- ImGui never writes `imgui.ini`: set `IniFilename` to null right after creating the context.
- The spike may leave `tools/zig/gfxgpu_smoke.zig` alone; its compile error (missing `format` field at line 142) is pre-existing and out of scope.

---

### Task 1: Overlay callback and frame capture in the renderer

**Files:**
- Modify: `Sources/src/GFXGPU/renderer.zig` (fields near line 78-94, `deinit` at 420, `endFrame` at 575, `readback` at 1475; tests at the end)
- Modify: `Sources/src/GFXGPU/sdl.zig` (one helper next to `blitTextureCentered`, line 224)

**Interfaces:**
- Produces (in `renderer.zig`):
  ```zig
  pub const OverlayCallback = *const fn (user: ?*anyopaque, command_buffer: *anyopaque, target: *anyopaque, width: u32, height: u32) callconv(.c) void;
  pub const Overlay = struct { callback: OverlayCallback, user: ?*anyopaque };
  // Renderer fields
  overlay: ?Overlay = null,
  capture_requested: bool = false,
  capture_ready: bool = false,
  capture_texture: ?*sdl.GpuTexture = null,
  capture_width: u32 = 0,
  capture_height: u32 = 0,
  // Renderer methods
  pub fn setOverlay(self: *Renderer, overlay: ?Overlay) !void          // error.InvalidState inside a frame
  pub fn requestFrameCapture(self: *Renderer, enabled: bool) !void     // error.InvalidState inside a frame
  pub fn readbackFrame(self: *Renderer, destination: []u8, width: u32, height: u32, row_pitch: u32) !void
  ```
- Produces (in `sdl.zig`): `pub fn blitTextureCopy(command_buffer: *GpuCommandBuffer, source: *GpuTexture, destination: *GpuTexture, width: u32, height: u32) void`

- [ ] **Step 1: Write the failing tests**

Append to the end of `Sources/src/GFXGPU/renderer.zig`. They use the fake device pattern of the existing test "the present mode is recorded only once the device takes it" (line ~1495): `device_mod.real_api` with overridden entries, a dummy handle, and `renderer.device = null` before `deinit` so no real SDL call hits the fakes. With `scene_texture == null` `endFrame` performs no blit, so no real SDL call is made.

```zig
test "the overlay runs once per frame, after the scene, on the frame's command buffer and target" {
    const Probe = struct {
        var calls: u32 = 0;
        var seen_command: ?*anyopaque = null;
        var seen_target: ?*anyopaque = null;
        var seen_width: u32 = 0;
        var seen_height: u32 = 0;
        var seen_user: ?*anyopaque = null;
        fn overlay(user: ?*anyopaque, command_buffer: *anyopaque, target: *anyopaque, width: u32, height: u32) callconv(.c) void {
            calls += 1;
            seen_user = user;
            seen_command = command_buffer;
            seen_target = target;
            seen_width = width;
            seen_height = height;
        }
        fn destroy(_: *anyopaque) void {}
    };
    Probe.calls = 0;
    var api = device_mod.real_api;
    api.destroy = Probe.destroy;
    var renderer = Renderer.init(std.testing.allocator);
    var handle: u8 = 0;
    var command: u8 = 0;
    var swapchain: u8 = 0;
    var user: u8 = 0;
    renderer.device = device_mod.Device{ .allocator = std.testing.allocator, .api = api, .handle = &handle };
    defer {
        renderer.device = null;
        renderer.deinit();
    }
    try renderer.setOverlay(.{ .callback = Probe.overlay, .user = &user });

    renderer.drawable_width = 640;
    renderer.drawable_height = 480;
    try renderer.frame.begin(true);
    renderer.frame.command_buffer = &command;
    renderer.frame.swapchain_texture = &swapchain;
    try renderer.endFrame();

    try std.testing.expectEqual(@as(u32, 1), Probe.calls);
    try std.testing.expectEqual(@as(?*anyopaque, &command), Probe.seen_command);
    try std.testing.expectEqual(@as(?*anyopaque, &swapchain), Probe.seen_target);
    try std.testing.expectEqual(@as(?*anyopaque, &user), Probe.seen_user);
    try std.testing.expectEqual(@as(u32, 640), Probe.seen_width);
    try std.testing.expectEqual(@as(u32, 480), Probe.seen_height);
    try std.testing.expectEqual(frame_mod.State.ready_to_submit, renderer.frame.state);
    renderer.frame.cancel();
}

test "a skipped frame runs no overlay, and a cleared overlay stays silent" {
    const Probe = struct {
        var calls: u32 = 0;
        fn overlay(_: ?*anyopaque, _: *anyopaque, _: *anyopaque, _: u32, _: u32) callconv(.c) void {
            calls += 1;
        }
    };
    Probe.calls = 0;
    var renderer = Renderer.init(std.testing.allocator);
    defer renderer.deinit();
    try renderer.setOverlay(.{ .callback = Probe.overlay, .user = null });
    renderer.frame.skipped = true;
    try renderer.endFrame();
    try std.testing.expectEqual(@as(u32, 0), Probe.calls);
    renderer.frame.cancel();

    try renderer.setOverlay(null);
    try std.testing.expect(renderer.overlay == null);
}

test "the overlay and the capture request cannot change inside a frame" {
    const Probe = struct {
        fn overlay(_: ?*anyopaque, _: *anyopaque, _: *anyopaque, _: u32, _: u32) callconv(.c) void {}
    };
    var renderer = Renderer.init(std.testing.allocator);
    defer renderer.deinit();
    renderer.frame.state = .recording;
    try std.testing.expectError(error.InvalidState, renderer.setOverlay(.{ .callback = Probe.overlay, .user = null }));
    try std.testing.expectError(error.InvalidState, renderer.requestFrameCapture(true));
    renderer.frame.state = .idle;
    try renderer.requestFrameCapture(true);
    try std.testing.expect(renderer.capture_requested);
}

test "reading a frame back before one was captured is refused" {
    var renderer = Renderer.init(std.testing.allocator);
    defer renderer.deinit();
    var pixels: [16]u8 = undefined;
    try std.testing.expectError(error.ReadbackUnavailable, renderer.readbackFrame(&pixels, 2, 2, 8));
}
```

If `frame_mod` is not already imported in `renderer.zig`, use the name under which `frame.zig` is imported there (check the imports at the top, e.g. `const frame_mod = @import("frame.zig");`) and add that import if it is missing.

- [ ] **Step 2: Run the tests and verify they fail**

Run: `zig build test-gfxgpu-core -Dtarget=aarch64-macos`
Expected: compile errors `no field or member function named 'setOverlay'` (and `requestFrameCapture`, `readbackFrame`, `overlay`, `capture_requested`).

- [ ] **Step 3: Add the copy blit to `sdl.zig`**

Insert after `blitTextureCentered` (line ~236):

```zig
// A 1:1 copy of a whole texture onto another of the same size. Used to move a
// captured frame onto the swapchain.
pub fn blitTextureCopy(command_buffer: *GpuCommandBuffer, source: *GpuTexture, destination: *GpuTexture, width: u32, height: u32) void {
    if (width == 0 or height == 0) return;
    const info = c.SDL_GPUBlitInfo{
        .source = .{ .texture = source, .w = width, .h = height },
        .destination = .{ .texture = destination, .w = width, .h = height },
        .load_op = c.SDL_GPU_LOADOP_DONT_CARE,
        .filter = c.SDL_GPU_FILTER_NEAREST,
    };
    c.SDL_BlitGPUTexture(command_buffer, &info);
}
```

Check the field names against `blitTextureCentered` right above it (it builds the same `SDL_GPUBlitInfo`) and match them exactly.

- [ ] **Step 4: Add the types and fields to the renderer**

At file scope in `renderer.zig`, above `pub const Renderer = struct`:

```zig
// Called once per presented frame, after the scene has been drawn onto the
// frame's colour target and before the command buffer is submitted. The
// callback owns any passes it opens on `command_buffer` and must end them
// before it returns; `target` is a colour target of width x height pixels in
// the swapchain's format. The editor draws its ImGui panels here.
pub const OverlayCallback = *const fn (user: ?*anyopaque, command_buffer: *anyopaque, target: *anyopaque, width: u32, height: u32) callconv(.c) void;
pub const Overlay = struct { callback: OverlayCallback, user: ?*anyopaque };
```

In `Renderer`, after `scene_depth: ?*sdl.GpuTexture = null,`:

```zig
    overlay: ?Overlay = null,
    // A capture composes the next frame (scene and overlay) into
    // capture_texture instead of the swapchain and then copies it across, so
    // what was presented can be read back. The swapchain itself is not
    // readable on every backend.
    capture_requested: bool = false,
    capture_ready: bool = false,
    capture_texture: ?*sdl.GpuTexture = null,
    capture_width: u32 = 0,
    capture_height: u32 = 0,
```

- [ ] **Step 5: Add the methods**

Inside `Renderer`, after `setViewport`:

```zig
    pub fn setOverlay(self: *Renderer, overlay: ?Overlay) !void {
        if (self.frame.state != .idle) return error.InvalidState;
        self.overlay = overlay;
    }

    pub fn requestFrameCapture(self: *Renderer, enabled: bool) !void {
        if (self.frame.state != .idle) return error.InvalidState;
        self.capture_requested = enabled;
    }

    fn ensureCaptureTexture(self: *Renderer, width: u32, height: u32) !*sdl.GpuTexture {
        const device = &(self.device orelse return error.NoDevice);
        const gpu_device: *sdl.GpuDevice = @ptrCast(@alignCast(device.handle.?));
        if (self.capture_texture) |texture| {
            if (self.capture_width == width and self.capture_height == height) return texture;
            sdl.releaseTexture(gpu_device, texture);
            self.capture_texture = null;
        }
        const texture = sdl.createCaptureTexture(gpu_device, @intCast(self.swapchain_format), width, height) orelse return error.CaptureTextureCreateFailed;
        self.capture_texture = texture;
        self.capture_width = width;
        self.capture_height = height;
        return texture;
    }
```

- [ ] **Step 6: Compose into the capture target and run the overlay in `endFrame`**

Replace the body of `endFrame` (line 575) with:

```zig
    pub fn endFrame(self: *Renderer) !void {
        if (self.frame.skipped) return;
        if (self.frame.render_pass) |pass| {
            const device = &(self.device orelse return error.NoDevice);
            device.api.end_render_pass(pass);
            self.frame.render_pass = null;
            self.frame.endPass() catch return error.InvalidState;
        }
        const command = self.frame.command_buffer orelse return error.InvalidState;
        const swapchain: *sdl.GpuTexture = @ptrCast(@alignCast(self.frame.swapchain_texture orelse return error.InvalidState));
        const capturing = self.capture_requested;
        const target: *sdl.GpuTexture = if (capturing) try self.ensureCaptureTexture(self.drawable_width, self.drawable_height) else swapchain;
        if (self.scene_texture) |scene| {
            if (self.present_fit)
                sdl.blitTextureFit(@ptrCast(@alignCast(command)), scene, self.scene_width, self.scene_height, target, self.drawable_width, self.drawable_height)
            else
                sdl.blitTextureCentered(@ptrCast(@alignCast(command)), scene, self.scene_width, self.scene_height, target, self.drawable_width, self.drawable_height);
        }
        if (self.overlay) |overlay| overlay.callback(overlay.user, command, @ptrCast(target), self.drawable_width, self.drawable_height);
        if (capturing) {
            sdl.blitTextureCopy(@ptrCast(@alignCast(command)), target, swapchain, self.drawable_width, self.drawable_height);
            self.capture_requested = false;
            self.capture_ready = true;
        }
        try self.frame.end();
    }
```

This keeps the old behaviour exactly when neither overlay nor capture is set (the scene is blitted straight onto the swapchain).

- [ ] **Step 7: Share the download code and add `readbackFrame`**

Replace `readback` (line 1475) with a shared helper and two thin entry points:

```zig
    fn downloadInto(self: *Renderer, texture: *sdl.GpuTexture, destination: []u8, width: u32, height: u32, row_pitch: u32) !void {
        if (width == 0 or height == 0 or row_pitch < width * 4 or destination.len < @as(usize, row_pitch) * height) return error.ReadbackInvalid;
        const device = &(self.device orelse return error.NoDevice);
        const gpu_device: *sdl.GpuDevice = @ptrCast(@alignCast(device.handle.?));
        const transfer = sdl.createDownloadBuffer(gpu_device, row_pitch * height) orelse return error.TransferBufferCreateFailed;
        defer sdl.releaseTransferBuffer(gpu_device, transfer);
        const command = sdl.acquireCommandBuffer(gpu_device) orelse return error.CommandBufferFailed;
        if (!sdl.downloadTexture(command, texture, transfer, width, height)) {
            _ = sdl.cancelCommandBuffer(command);
            return error.CopyPassFailed;
        }
        if (!sdl.submitCommandBuffer(command)) return error.SubmitFailed;
        if (!sdl.waitForIdle(gpu_device)) return error.WaitForIdleFailed;
        const mapped = sdl.mapTransferBuffer(gpu_device, transfer) orelse return error.TransferBufferMapFailed;
        @memcpy(destination[0 .. @as(usize, row_pitch) * height], @as([*]const u8, @ptrCast(mapped))[0 .. @as(usize, row_pitch) * height]);
        sdl.unmapTransferBuffer(gpu_device, transfer);
    }

    pub fn readback(self: *Renderer, destination: []u8, width: u32, height: u32, row_pitch: u32) !void {
        const texture = self.scene_texture orelse return error.ReadbackUnavailable;
        return self.downloadInto(texture, destination, width, height, row_pitch);
    }

    // The last captured frame as presented: scene plus overlay, at drawable
    // size, in the swapchain's format (BGRA8 on Metal and Direct3D).
    pub fn readbackFrame(self: *Renderer, destination: []u8, width: u32, height: u32, row_pitch: u32) !void {
        if (!self.capture_ready) return error.ReadbackUnavailable;
        const texture = self.capture_texture orelse return error.ReadbackUnavailable;
        if (width != self.capture_width or height != self.capture_height) return error.ReadbackInvalid;
        return self.downloadInto(texture, destination, width, height, row_pitch);
    }
```

- [ ] **Step 8: Release the capture texture**

In `deinit`, after `if (self.scene_depth) |texture| sdl.releaseTexture(gpu_device, texture);` add:

```zig
            if (self.capture_texture) |texture| sdl.releaseTexture(gpu_device, texture);
```

- [ ] **Step 9: Run the tests and verify they pass**

Run: `zig build test-gfxgpu-core -Dtarget=aarch64-macos`
Expected: all tests pass, including the four new ones. `abi.zig`'s `gfxgpu_readback` switches exhaustively over `readback`'s error set; that set must be unchanged (`ReadbackUnavailable`, `ReadbackInvalid`, `NoDevice`, `TransferBufferCreateFailed`, `CommandBufferFailed`, `CopyPassFailed`, `SubmitFailed`, `WaitForIdleFailed`, `TransferBufferMapFailed`). A compile error there means a new error leaked into `downloadInto`; remove it rather than extending the switch.

- [ ] **Step 10: Commit**

```bash
git add Sources/src/GFXGPU/renderer.zig Sources/src/GFXGPU/sdl.zig
git commit -m "feat(gfxgpu): an overlay callback after the scene and a capture of the presented frame" -m "Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: Overlay and capture in the GFXGPU C ABI

**Files:**
- Modify: `Sources/src/GFXGPU/abi.zig` (`Api` at line 62-100, entry points near `setPresentMode` line 452, `gfxgpu_readback` line 541, the `api` table line 556-592, tests after line 620)
- Modify: `Sources/src/GFXGPU/root.zig` (export next to `gfxgpu_readback`)
- Modify: `Sources/src/GFXGPU/gfxgpu_c.h` (`GfxGpuApi` line 150-196, prototypes line 197-198)
- Modify: `tools/zig/gfxgpu_abi_test.cpp` (after the table size check, line ~12)

**Interfaces:**
- Consumes: `Renderer.setOverlay`, `Renderer.requestFrameCapture`, `Renderer.readbackFrame`, `renderer_mod.OverlayCallback` from Task 1.
- Produces (appended to `Api`, in this order):
  ```zig
  set_overlay: *const fn (?*RendererHandle, ?renderer_mod.OverlayCallback, ?*anyopaque) callconv(.c) Result,
  get_gpu_device: *const fn (?*RendererHandle, ?*?*anyopaque, ?*u32) callconv(.c) Result,
  set_frame_capture: *const fn (?*RendererHandle, u32) callconv(.c) Result,
  ```
  and the export `gfxgpu_readback_frame(handle: ?*RendererHandle, info: ?*ReadbackInfo) callconv(.c) Result`.
- C mirror:
  ```c
  typedef void (*GfxGpuOverlayCallback)(void *user, void *command_buffer, void *target, uint32_t width, uint32_t height);
  GfxGpuResult (*set_overlay)(GfxGpuRenderer *, GfxGpuOverlayCallback, void *);
  GfxGpuResult (*get_gpu_device)(GfxGpuRenderer *, void **, uint32_t *);
  GfxGpuResult (*set_frame_capture)(GfxGpuRenderer *, uint32_t);
  GfxGpuResult gfxgpu_readback_frame(GfxGpuRenderer *, GfxGpuReadbackInfo *);
  ```

- [ ] **Step 1: Write the failing tests**

Append to `abi.zig` after the test "the API table is filled to the caller's size and never past it":

```zig
test "the overlay, device and capture entry points are in the table and validate their arguments" {
    var table: Api = undefined;
    table.struct_size = @sizeOf(Api);
    try std.testing.expectEqual(errors.ok, gfxgpu_get_api(abi_version, &table));
    try std.testing.expectEqual(@as(u32, @sizeOf(Api)), table.struct_size);

    try std.testing.expectEqual(errors.invalid_handle, table.set_overlay(null, null, null));
    try std.testing.expectEqual(errors.invalid_handle, table.set_frame_capture(null, 1));
    var device: ?*anyopaque = null;
    var format: u32 = 0;
    try std.testing.expectEqual(errors.invalid_handle, table.get_gpu_device(null, &device, &format));

    var renderer = renderer_mod.Renderer.init(std.testing.allocator);
    defer renderer.deinit();
    const handle: *RendererHandle = @ptrCast(&renderer);
    // No device yet: there is nothing to hand out.
    try std.testing.expectEqual(errors.invalid_state, table.get_gpu_device(handle, &device, &format));
    try std.testing.expectEqual(errors.invalid_argument, table.get_gpu_device(handle, null, &format));
    try std.testing.expectEqual(errors.ok, table.set_frame_capture(handle, 1));
    try std.testing.expect(renderer.capture_requested);
    try std.testing.expectEqual(errors.ok, table.set_overlay(handle, null, null));

    var pixels: [16]u8 = undefined;
    var info = ReadbackInfo{ .struct_size = @sizeOf(ReadbackInfo), .width = 2, .height = 2, .byte_length = pixels.len, .row_pitch = 8, .data = @ptrCast(&pixels) };
    try std.testing.expectEqual(errors.unsupported, gfxgpu_readback_frame(handle, &info));
}
```

- [ ] **Step 2: Run the tests and verify they fail**

Run: `zig build test-gfxgpu-core -Dtarget=aarch64-macos`
Expected: compile error `no field named 'set_overlay' in struct 'abi.Api'`.

- [ ] **Step 3: Append the entries to `Api`**

In `abi.zig`, after `set_present_mode: ...,` inside `pub const Api = extern struct`:

```zig
    // Appended: the editor's overlay. Runs after the scene, before submit;
    // see renderer.OverlayCallback. Null clears it. Refused inside a frame.
    set_overlay: *const fn (?*RendererHandle, ?renderer_mod.OverlayCallback, ?*anyopaque) callconv(.c) Result,
    // Appended: the SDL_GPUDevice* and the swapchain's SDL_GPUTextureFormat,
    // so an overlay can create its own pipelines on the same device.
    get_gpu_device: *const fn (?*RendererHandle, ?*?*anyopaque, ?*u32) callconv(.c) Result,
    // Appended: nonzero captures the next presented frame for
    // gfxgpu_readback_frame. Refused inside a frame.
    set_frame_capture: *const fn (?*RendererHandle, u32) callconv(.c) Result,
```

- [ ] **Step 4: Add the entry points**

After `setPresentMode` (line ~456):

```zig
fn setOverlay(handle: ?*RendererHandle, callback: ?renderer_mod.OverlayCallback, user: ?*anyopaque) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    const overlay: ?renderer_mod.Overlay = if (callback) |value| .{ .callback = value, .user = user } else null;
    renderer.setOverlay(overlay) catch return errors.invalid_state;
    return errors.ok;
}
fn getGpuDevice(handle: ?*RendererHandle, out_device: ?*?*anyopaque, out_format: ?*u32) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    if (out_device == null or out_format == null) return errors.invalid_argument;
    const device = renderer.device orelse return errors.invalid_state;
    out_device.?.* = device.handle;
    out_format.?.* = renderer.swapchain_format;
    return errors.ok;
}
fn setFrameCapture(handle: ?*RendererHandle, enabled: u32) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    renderer.requestFrameCapture(enabled != 0) catch return errors.invalid_state;
    return errors.ok;
}
```

After `gfxgpu_readback` (line ~554):

```zig
pub fn gfxgpu_readback_frame(handle: ?*RendererHandle, info: ?*ReadbackInfo) callconv(.c) Result {
    const renderer = withRenderer(handle) orelse return errors.invalid_handle;
    if (info == null or info.?.struct_size < @sizeOf(ReadbackInfo) or info.?.width == 0 or info.?.height == 0 or info.?.data == null) return errors.invalid_argument;
    if (info.?.row_pitch < info.?.width * 4 or info.?.byte_length < info.?.row_pitch * info.?.height) return errors.invalid_argument;
    renderer.readbackFrame(@as([*]u8, @ptrCast(info.?.data.?))[0..info.?.byte_length], info.?.width, info.?.height, info.?.row_pitch) catch |err| {
        renderer.last_error = @errorName(err);
        return switch (err) {
            error.ReadbackUnavailable => errors.unsupported,
            error.ReadbackInvalid => errors.invalid_state,
            error.NoDevice, error.TransferBufferCreateFailed, error.CommandBufferFailed, error.CopyPassFailed, error.SubmitFailed, error.WaitForIdleFailed, error.TransferBufferMapFailed => errors.sdl_error,
        };
    };
    return errors.ok;
}
```

In the `api` table, after `.set_present_mode = setPresentMode,`:

```zig
    .set_overlay = setOverlay,
    .get_gpu_device = getGpuDevice,
    .set_frame_capture = setFrameCapture,
```

In `root.zig`, after the `gfxgpu_readback` export:

```zig
pub export fn gfxgpu_readback_frame(handle: ?*abi.RendererHandle, info: ?*abi.ReadbackInfo) callconv(.c) abi.Result {
    return abi.gfxgpu_readback_frame(handle, info);
}
```

- [ ] **Step 5: Mirror the C header**

In `gfxgpu_c.h`, before `typedef struct GfxGpuApi {`:

```c
/* Called once per presented frame after the scene, before submit. The
   callback owns and must end any pass it opens on command_buffer
   (an SDL_GPUCommandBuffer*); target is an SDL_GPUTexture* colour target of
   width x height pixels in the swapchain's format. */
typedef void (*GfxGpuOverlayCallback)(void *user, void *command_buffer, void *target, uint32_t width, uint32_t height);
```

Inside `GfxGpuApi`, after `set_present_mode`:

```c
    /* Appended: the editor's overlay; NULL clears it. Refused inside a frame. */
    GfxGpuResult (*set_overlay)(GfxGpuRenderer *, GfxGpuOverlayCallback, void *);
    /* Appended: the SDL_GPUDevice* and the swapchain's SDL_GPUTextureFormat. */
    GfxGpuResult (*get_gpu_device)(GfxGpuRenderer *, void **, uint32_t *);
    /* Appended: nonzero captures the next presented frame for
       gfxgpu_readback_frame. Refused inside a frame. */
    GfxGpuResult (*set_frame_capture)(GfxGpuRenderer *, uint32_t);
```

After the `gfxgpu_readback` prototype:

```c
/* The last captured frame as presented (scene and overlay), at drawable
   size, in the swapchain's byte order. GFXGPU_UNSUPPORTED before a capture. */
GfxGpuResult gfxgpu_readback_frame(GfxGpuRenderer *, GfxGpuReadbackInfo *);
```

- [ ] **Step 6: Extend the C++ ABI test**

In `tools/zig/gfxgpu_abi_test.cpp`, after the check `if ( api.abi_version != GFXGPU_ABI_VERSION || api.struct_size != sizeof( api ) ) return 2;`, add (pick return codes not already used in the file; check with `grep -n "return [0-9]" tools/zig/gfxgpu_abi_test.cpp`):

```cpp
    // The editor's entry points reach C callers in the header's order.
    if ( api.set_overlay == nullptr || api.get_gpu_device == nullptr || api.set_frame_capture == nullptr ) return 40;
    if ( api.set_overlay( nullptr, nullptr, nullptr ) != GFXGPU_INVALID_HANDLE ) return 41;
```

Check the exact name of the invalid-handle constant in `gfxgpu_c.h` (`grep -n "INVALID_HANDLE" Sources/src/GFXGPU/gfxgpu_c.h`) and use it.

- [ ] **Step 7: Run the tests and verify they pass**

Run: `zig build test-gfxgpu-core -Dtarget=aarch64-macos && zig build gfxgpu-abi-test -Dtarget=aarch64-macos`
Expected: both succeed. The existing test "the API table is filled to the caller's size and never past it" still passes (appended entries lie past `api_base_size`).

- [ ] **Step 8: Check that the game still builds against the grown table**

Run: `zig build install-game -Dtarget=aarch64-macos --release=fast`
Expected: success (the C++ `GraphicsEngineGpu` compiles against the new header). Staging may fail afterwards with FileNotFound for `config.cfg` in this worktree because it has no full Data; the compile and link must succeed.

- [ ] **Step 9: Commit**

```bash
git add Sources/src/GFXGPU/abi.zig Sources/src/GFXGPU/root.zig Sources/src/GFXGPU/gfxgpu_c.h tools/zig/gfxgpu_abi_test.cpp
git commit -m "feat(gfxgpu): overlay, device and frame capture entry points in the C ABI" -m "Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: Vendor Dear ImGui (dcimgui docking) and its SDL3 backends

**Files:**
- Create: `vendor/dcimgui/` (from `floooh/dcimgui` tag `v1.92.9b`, directory `src-docking/` only, plus `LICENSE`)
- Create: `vendor/dcimgui/backends/` (from `ocornut/imgui` tag `v1.92.9b-docking`: `backends/imgui_impl_sdl3.{h,cpp}`, `backends/imgui_impl_sdlgpu3.{h,cpp}`, `backends/imgui_impl_sdlgpu3_shaders.h`)
- Create: `vendor/dcimgui/BLITZKRIEG_VENDOR.md`
- Modify: `build.zig` (new function `addEditorImgui`, called after `sdl3` is created near line 1365; new step `editor-imgui`)

**Interfaces:**
- Produces: `fn addEditorImgui(b: *std.Build, target: std.Build.ResolvedTarget, optimize: std.builtin.OptimizeMode, toolchain: <type of the existing toolchain value>, sdl_include: std.Build.LazyPath) *std.Build.Step.Compile` returning a static library named `editor-imgui` that contains ImGui, the dcimgui C bindings, both backends and (from Task 4) the shim.
- Produces: build step `editor-imgui` ("Build Dear ImGui with its SDL3 backends for the editor").

- [ ] **Step 1: Fetch the sources at the pinned tags**

```bash
cd /Users/johannes/Projects/src/Blitzkrieg/.worktrees/map-editor
mkdir -p zig-out/local-test/vendor-fetch && cd zig-out/local-test/vendor-fetch
git clone --depth 1 --branch v1.92.9b https://github.com/floooh/dcimgui.git dcimgui
git clone --depth 1 --branch v1.92.9b-docking https://github.com/ocornut/imgui.git imgui
git -C dcimgui rev-parse HEAD
git -C imgui rev-parse HEAD
cd ../../..
mkdir -p vendor/dcimgui/backends
cp -R zig-out/local-test/vendor-fetch/dcimgui/src-docking vendor/dcimgui/src-docking
cp zig-out/local-test/vendor-fetch/dcimgui/LICENSE vendor/dcimgui/LICENSE
for f in imgui_impl_sdl3.h imgui_impl_sdl3.cpp imgui_impl_sdlgpu3.h imgui_impl_sdlgpu3.cpp imgui_impl_sdlgpu3_shaders.h; do
  cp zig-out/local-test/vendor-fetch/imgui/backends/$f vendor/dcimgui/backends/$f
done
cp zig-out/local-test/vendor-fetch/imgui/LICENSE.txt vendor/dcimgui/backends/LICENSE.txt
```

Expected: the two `rev-parse` lines print commits; note them for the vendor record. Also check the pairing is right: `grep -n "IMGUI_VERSION " vendor/dcimgui/src-docking/imgui.h` must print `"1.92.9b"`, and `grep -n "IMGUI_HAS_DOCK" vendor/dcimgui/src-docking/imgui.h` must find the docking define.

- [ ] **Step 2: Write the vendor record**

`vendor/dcimgui/BLITZKRIEG_VENDOR.md`:

```markdown
# Blitzkrieg Vendor Record: dcimgui (Dear ImGui, docking branch)

Sources:

- `src-docking/`, `LICENSE`: https://github.com/floooh/dcimgui, tag `v1.92.9b`, commit `<from step 1>`.
  Dear ImGui 1.92.9b (docking branch) with the dear_bindings C API (`cimgui.h`, `ig*` functions).
- `backends/`: https://github.com/ocornut/imgui, tag `v1.92.9b-docking`, commit `<from step 1>`.
  `imgui_impl_sdl3` (platform) and `imgui_impl_sdlgpu3` (SDL GPU renderer, shaders embedded).

Local integration patches: none.

The backends must come from the same Dear ImGui version and branch as
`src-docking/`; a mismatch can compile and then fail at run time. Refreshing
means updating both sources to matching tags and repeating the version check
(`IMGUI_VERSION` in `src-docking/imgui.h`).

Used by the portable Map Editor (`docs/superpowers/specs/2026-09-19-portable-map-editor-design.md`).
```

Replace both `<from step 1>` with the printed commits.

- [ ] **Step 3: Add the library to `build.zig`**

Add the function next to `addGfxGpuZig` (line ~3812). Use the same helpers as `gfx_gpu_abi_test_module` (line ~1545) for the C++ toolchain on Windows and Linux; check the exact type of `toolchain` in the signature of an existing function such as `addGameBootstrapSmoke` and use it here.

```zig
// Dear ImGui (docking branch) with the dear_bindings C API and the SDL3 +
// SDL GPU backends, for the portable editors. Static: it lives inside the
// editor executable and shares the one dynamic SDL3 the game ships.
fn addEditorImgui(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: anytype,
    sdl_include: std.Build.LazyPath,
) *std.Build.Step.Compile {
    const module = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libcpp = target.result.os.tag != .windows,
    });
    module.addIncludePath(b.path("vendor/dcimgui/src-docking"));
    module.addIncludePath(b.path("vendor/dcimgui/backends"));
    module.addIncludePath(b.path("Sources/editor/imgui"));
    module.addIncludePath(sdl_include);
    addMsvcIncludePaths(b, module, toolchain);
    addLinuxCxxIncludePaths(b, module);
    addMsvcLibraryPaths(b, module, toolchain);
    linkMsvcRuntime(module, optimize);
    module.addCSourceFiles(.{
        .files = &.{
            "vendor/dcimgui/src-docking/imgui.cpp",
            "vendor/dcimgui/src-docking/imgui_demo.cpp",
            "vendor/dcimgui/src-docking/imgui_draw.cpp",
            "vendor/dcimgui/src-docking/imgui_tables.cpp",
            "vendor/dcimgui/src-docking/imgui_widgets.cpp",
            "vendor/dcimgui/src-docking/cimgui.cpp",
            "vendor/dcimgui/backends/imgui_impl_sdl3.cpp",
            "vendor/dcimgui/backends/imgui_impl_sdlgpu3.cpp",
        },
        .flags = cppflagsForTarget(target, optimize),
    });
    return b.addLibrary(.{ .name = "editor-imgui", .linkage = .static, .root_module = module });
}
```

If `addMsvcIncludePaths` and friends already guard on the target themselves (read them at line ~3946-4030), call them unconditionally as above; if they do not, wrap the MSVC ones in `if (target.result.os.tag == .windows)` the way the call sites near line 1130 do.

After `const gfx_gpu_zig = addGfxGpuZig(b, target, optimize, sdl3);` (line ~1366):

```zig
    const editor_imgui = addEditorImgui(b, target, optimize, toolchain, sdl_dynamic_dep.path("include"));
    const editor_imgui_step = b.step("editor-imgui", "Build Dear ImGui with its SDL3 backends for the editor");
    editor_imgui_step.dependOn(&editor_imgui.step);
```

- [ ] **Step 4: Build it**

Run: `zig build editor-imgui -Dtarget=aarch64-macos`
Expected: success. Warnings from ImGui sources are acceptable; errors are not. If `imgui_impl_sdlgpu3.cpp` cannot find `SDL3/SDL_gpu.h`, the SDL include path is wrong: check that the value passed is `sdl_dynamic_dep.path("include")`, the same one passed to `addGameBootstrapSmoke` on line ~1367.

- [ ] **Step 5: Commit**

```bash
git add vendor/dcimgui build.zig
git commit -m "build(editor): vendor Dear ImGui 1.92.9b docking with dcimgui C bindings and SDL3 backends" -m "Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 4: The C backend shim, the Zig ImGui module, and the spike program

**Files:**
- Create: `Sources/editor/imgui/imgui_backend.h`
- Create: `Sources/editor/imgui/imgui_backend.cpp`
- Create: `Sources/editor/imgui/imgui.zig`
- Create: `tools/zig/editor_overlay_spike.zig`
- Modify: `build.zig` (add the shim to `addEditorImgui`; new steps `editor-overlay-spike-build` and `editor-overlay-spike`)

**Interfaces:**
- Consumes: GFXGPU `Api.set_overlay`, `Api.get_gpu_device`, `Api.set_frame_capture`, `gfxgpu_readback_frame` (Task 2); `editor-imgui` library (Task 3).
- Produces (C, `imgui_backend.h`; SDL types passed as `void *` so Zig's `@cImport` of this header never sees SDL declarations that clash with the `sdl3` module):
  ```c
  bool bk_imgui_backend_init(void *sdl_window, void *gpu_device, uint32_t color_target_format);
  void bk_imgui_backend_shutdown(void);
  bool bk_imgui_backend_process_event(const void *sdl_event);
  void bk_imgui_backend_new_frame(void);
  void bk_imgui_backend_render(void *command_buffer, void *target);
  ```
- Produces (Zig module `editor_imgui`, `Sources/editor/imgui/imgui.zig`): `pub const c` (the `@cImport` of `cimgui.h` and `imgui_backend.h`) and
  `pub fn overlayCallback(user: ?*anyopaque, command_buffer: *anyopaque, target: *anyopaque, width: u32, height: u32) callconv(.c) void` for `set_overlay`.
- Produces: `zig build editor-overlay-spike` runs the spike and fails the step on a failed check.

- [ ] **Step 1: Write the shim header**

`Sources/editor/imgui/imgui_backend.h`:

```c
#ifndef BK_EDITOR_IMGUI_BACKEND_H
#define BK_EDITOR_IMGUI_BACKEND_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Dear ImGui's SDL3 platform backend and SDL GPU renderer backend behind a C
   API, for the Zig editors. SDL objects are passed as void * so this header
   stays free of SDL declarations. The ImGui context must exist (igCreateContext)
   before init. */

/* sdl_window: SDL_Window*; gpu_device: SDL_GPUDevice*; color_target_format:
   the SDL_GPUTextureFormat of the targets render() draws into. */
bool bk_imgui_backend_init(void *sdl_window, void *gpu_device, uint32_t color_target_format);
void bk_imgui_backend_shutdown(void);
/* sdl_event: const SDL_Event*. Returns true when ImGui used it. */
bool bk_imgui_backend_process_event(const void *sdl_event);
/* Both backends' NewFrame; call before igNewFrame. */
void bk_imgui_backend_new_frame(void);
/* Uploads the current draw data (igRender must have run) and draws it onto
   target with a LOAD pass, so what is already there stays. command_buffer:
   SDL_GPUCommandBuffer*; target: SDL_GPUTexture*. No pass may be open. */
void bk_imgui_backend_render(void *command_buffer, void *target);

#ifdef __cplusplus
}
#endif

#endif
```

- [ ] **Step 2: Write the shim**

`Sources/editor/imgui/imgui_backend.cpp`:

```cpp
#include "imgui_backend.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"

#include <SDL3/SDL.h>

extern "C" bool bk_imgui_backend_init( void *sdl_window, void *gpu_device, uint32_t color_target_format )
{
    SDL_Window *window = static_cast<SDL_Window *>( sdl_window );
    if ( !ImGui_ImplSDL3_InitForSDLGPU( window ) )
        return false;
    ImGui_ImplSDLGPU3_InitInfo info;
    info.Device = static_cast<SDL_GPUDevice *>( gpu_device );
    info.ColorTargetFormat = static_cast<SDL_GPUTextureFormat>( color_target_format );
    info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    if ( !ImGui_ImplSDLGPU3_Init( &info ) )
    {
        ImGui_ImplSDL3_Shutdown();
        return false;
    }
    return true;
}

extern "C" void bk_imgui_backend_shutdown( void )
{
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
}

extern "C" bool bk_imgui_backend_process_event( const void *sdl_event )
{
    return ImGui_ImplSDL3_ProcessEvent( static_cast<const SDL_Event *>( sdl_event ) );
}

extern "C" void bk_imgui_backend_new_frame( void )
{
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
}

extern "C" void bk_imgui_backend_render( void *command_buffer, void *target )
{
    ImDrawData *draw_data = ImGui::GetDrawData();
    if ( draw_data == nullptr || draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f )
        return;
    SDL_GPUCommandBuffer *command = static_cast<SDL_GPUCommandBuffer *>( command_buffer );
    // Uploads happen in a copy pass, which must not overlap a render pass.
    ImGui_ImplSDLGPU3_PrepareDrawData( draw_data, command );
    SDL_GPUColorTargetInfo color = {};
    color.texture = static_cast<SDL_GPUTexture *>( target );
    color.load_op = SDL_GPU_LOADOP_LOAD;
    color.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass( command, &color, 1, nullptr );
    if ( pass == nullptr )
        return;
    ImGui_ImplSDLGPU3_RenderDrawData( draw_data, command, pass );
    SDL_EndGPURenderPass( pass );
}
```

Add `"Sources/editor/imgui/imgui_backend.cpp",` to the file list in `addEditorImgui`.

- [ ] **Step 3: Write the Zig module**

`Sources/editor/imgui/imgui.zig`:

```zig
//! Dear ImGui for the Zig editors: the dcimgui C API (`ig*` functions,
//! docking branch) and the SDL3 / SDL GPU backend shim.
pub const c = @cImport({
    @cInclude("cimgui.h");
    @cInclude("imgui_backend.h");
});

/// GFXGPU overlay callback (`Api.set_overlay`): draws the ImGui frame that
/// the last `igRender` finished onto the renderer's colour target.
pub fn overlayCallback(user: ?*anyopaque, command_buffer: *anyopaque, target: *anyopaque, width: u32, height: u32) callconv(.c) void {
    _ = user;
    _ = width;
    _ = height;
    c.bk_imgui_backend_render(command_buffer, target);
}
```

- [ ] **Step 4: Write the spike program**

`tools/zig/editor_overlay_spike.zig`. It opens a window, starts GFXGPU on it, clears the scene to green, draws one opaque magenta ImGui window at (20, 20) sized 100x80 through the overlay, captures that frame, and checks one pixel inside the panel and one outside. Magenta and green read the same in RGBA and BGRA, so the check holds for any swapchain byte order.

```zig
//! Overlay spike for the portable Map Editor: proves that Dear ImGui, called
//! from Zig, draws on top of a GFXGPU frame in the same frame, and measures
//! both from one readback. Usage:
//!   editor-overlay-spike [--hidden] [--out <file.rgba>]
//! Exit code 0 and "overlay-spike: PASS" on success.
const std = @import("std");
const sdl3 = @import("sdl3");
const gpu = @import("gfxgpu");
const imgui = @import("editor_imgui");

const window_width = 320;
const window_height = 240;
const panel = struct {
    const x = 20;
    const y = 20;
    const w = 100;
    const h = 80;
};

const Pixel = struct { r: u8, g: u8, b: u8 };

fn pixelAt(pixels: []const u8, row_pitch: u32, x: u32, y: u32) Pixel {
    const i = @as(usize, y) * row_pitch + @as(usize, x) * 4;
    // Byte 1 is green in both RGBA and BGRA; bytes 0 and 2 swap, which the
    // checks below do not care about (magenta and green are symmetric).
    return .{ .r = pixels[i], .g = pixels[i + 1], .b = pixels[i + 2] };
}

fn isMagenta(p: Pixel) bool {
    return p.r > 200 and p.g < 60 and p.b > 200;
}

fn isGreen(p: Pixel) bool {
    return p.g > 200 and p.r < 60 and p.b < 60;
}

pub fn main(init: std.process.Init) !void {
    var hidden = false;
    var out_path: ?[]const u8 = null;
    var args = try std.process.Args.Iterator.initAllocator(init.minimal.args, init.gpa);
    defer args.deinit();
    _ = args.next();
    while (args.next()) |arg| {
        if (std.mem.eql(u8, arg, "--hidden")) {
            hidden = true;
        } else if (std.mem.eql(u8, arg, "--out")) {
            out_path = args.next() orelse return error.MissingOutPath;
        } else return error.UnexpectedArgument;
    }

    if (!sdl3.c.SDL_Init(sdl3.c.SDL_INIT_VIDEO)) return error.SdlInitFailed;
    defer sdl3.c.SDL_Quit();
    const flags: sdl3.c.SDL_WindowFlags = if (hidden) sdl3.c.SDL_WINDOW_HIDDEN else 0;
    const window = sdl3.c.SDL_CreateWindow("Map Editor overlay spike", window_width, window_height, flags) orelse return error.SdlWindowFailed;
    defer sdl3.c.SDL_DestroyWindow(window);
    if (!hidden) _ = sdl3.c.SDL_ShowWindow(window);
    sdl3.c.SDL_PumpEvents();

    var api: gpu.abi.Api = undefined;
    api.struct_size = @sizeOf(gpu.abi.Api);
    if (gpu.abi.gfxgpu_get_api(gpu.abi.abi_version, &api) != gpu.error_codes.ok) return error.GfxGpuApiFailed;
    var renderer: ?*gpu.abi.RendererHandle = null;
    var create_info = gpu.abi.CreateInfo{ .struct_size = @sizeOf(gpu.abi.CreateInfo), .flags = 0, .sdl_window = @ptrCast(window), .width = window_width, .height = window_height, .shader_directory_utf8 = "zig-out/shaders", .preferred_driver_utf8 = null };
    if (api.create(&create_info, &renderer) != gpu.error_codes.ok) {
        std.debug.print("overlay-spike: renderer create failed: SDL={s}\n", .{std.mem.span(sdl3.c.SDL_GetError())});
        return error.RendererCreateFailed;
    }
    defer api.destroy(renderer);

    var device: ?*anyopaque = null;
    var format: u32 = 0;
    if (api.get_gpu_device(renderer, &device, &format) != gpu.error_codes.ok) return error.GpuDeviceUnavailable;

    _ = imgui.c.igCreateContext(null);
    defer imgui.c.igDestroyContext(null);
    imgui.c.igGetIO().*.IniFilename = null;
    if (!imgui.c.bk_imgui_backend_init(@ptrCast(window), device, format)) return error.ImguiBackendInitFailed;
    defer imgui.c.bk_imgui_backend_shutdown();
    if (api.set_overlay(renderer, imgui.overlayCallback, null) != gpu.error_codes.ok) return error.SetOverlayFailed;

    // The capture has the drawable's size, which the renderer refreshes on
    // every swapchain acquire and which equals the window's size in pixels.
    // If gfxgpu_readback_frame answers invalid_state, the two differ: query
    // the size again after the first presented frame.
    var pixel_width: c_int = 0;
    var pixel_height: c_int = 0;
    _ = sdl3.c.SDL_GetWindowSizeInPixels(window, &pixel_width, &pixel_height);
    const width: u32 = @intCast(pixel_width);
    const height: u32 = @intCast(pixel_height);
    const scale: f32 = @as(f32, @floatFromInt(width)) / window_width;

    const row_pitch = width * 4;
    const pixels = try init.gpa.alloc(u8, @as(usize, row_pitch) * height);
    defer init.gpa.free(pixels);

    var captured = false;
    var presented: u32 = 0;
    var attempt: u32 = 0;
    while (attempt < 120 and !captured) : (attempt += 1) {
        var event: sdl3.c.SDL_Event = undefined;
        while (sdl3.c.SDL_PollEvent(&event)) _ = imgui.c.bk_imgui_backend_process_event(@ptrCast(&event));

        imgui.c.bk_imgui_backend_new_frame();
        imgui.c.igNewFrame();
        imgui.c.igSetNextWindowPos(.{ .x = panel.x, .y = panel.y }, imgui.c.ImGuiCond_Always);
        imgui.c.igSetNextWindowSize(.{ .x = panel.w, .y = panel.h }, imgui.c.ImGuiCond_Always);
        imgui.c.igPushStyleColorImVec4(imgui.c.ImGuiCol_WindowBg, .{ .x = 1, .y = 0, .z = 1, .w = 1 });
        _ = imgui.c.igBegin("probe", null, imgui.c.ImGuiWindowFlags_NoDecoration | imgui.c.ImGuiWindowFlags_NoMove | imgui.c.ImGuiWindowFlags_NoSavedSettings);
        imgui.c.igEnd();
        imgui.c.igPopStyleColor();
        imgui.c.igRender();

        // Let a few frames through first so the font atlas is uploaded and
        // the window has settled on screen.
        const capture_this = presented >= 3;
        if (capture_this) _ = api.set_frame_capture(renderer, 1);
        if (api.begin_frame(renderer) != gpu.error_codes.ok) {
            // Skipped (no swapchain texture this time, e.g. occluded): try again.
            if (capture_this) _ = api.set_frame_capture(renderer, 0);
            sdl3.c.SDL_Delay(16);
            continue;
        }
        var viewport = gpu.abi.ViewportInfo{ .struct_size = @sizeOf(gpu.abi.ViewportInfo), .x = 0, .y = 0, .width = window_width, .height = window_height, .min_depth = 0, .max_depth = 1 };
        _ = api.set_viewport(renderer, &viewport);
        // D3DCOLOR 0xAARRGGBB: opaque green.
        var clear = gpu.abi.ClearInfo{ .struct_size = @sizeOf(gpu.abi.ClearInfo), .mask = 1, .color_rgba8 = 0xff00ff00, .depth = 1.0, .stencil = 0 };
        if (api.clear(renderer, &clear) != gpu.error_codes.ok) return error.ClearFailed;
        if (api.end_frame(renderer) != gpu.error_codes.ok) return error.EndFrameFailed;
        if (api.present(renderer) != gpu.error_codes.ok) return error.PresentFailed;
        presented += 1;
        if (capture_this) {
            var info = gpu.abi.ReadbackInfo{ .struct_size = @sizeOf(gpu.abi.ReadbackInfo), .width = width, .height = height, .byte_length = @intCast(pixels.len), .row_pitch = row_pitch, .data = pixels.ptr };
            if (gpu.abi.gfxgpu_readback_frame(renderer, &info) != gpu.error_codes.ok) return error.ReadbackFailed;
            captured = true;
        }
    }
    if (!captured) {
        std.debug.print("overlay-spike: FAIL no frame could be presented in {} attempts (hidden={})\n", .{ attempt, hidden });
        std.process.exit(2);
    }

    if (out_path) |path| {
        try std.Io.Dir.cwd().writeFile(init.io, .{ .sub_path = path, .data = pixels });
    }

    const inside = pixelAt(pixels, row_pitch, @intFromFloat((panel.x + panel.w / 2) * scale), @intFromFloat((panel.y + panel.h / 2) * scale));
    const outside = pixelAt(pixels, row_pitch, width - @as(u32, @intFromFloat(10 * scale)), height - @as(u32, @intFromFloat(10 * scale)));
    const pass = isMagenta(inside) and isGreen(outside);
    std.debug.print("overlay-spike: {s} format={} size={}x{} hidden={} inside=({},{},{}) outside=({},{},{})\n", .{
        if (pass) "PASS" else "FAIL", format, width, height, hidden,
        inside.r, inside.g, inside.b, outside.r, outside.g, outside.b,
    });
    if (!pass) std.process.exit(1);
}
```

If `std.Io.Dir.cwd().writeFile` has a different shape in this Zig 0.16 build, look at how `tools/zig/gfxgpu_smoke.zig` or another tool under `tools/zig/` writes a file (`grep -rn "writeFile\|createFile" tools/zig/*.zig | head`) and use the same call. If `igGetIO().*.IniFilename` does not compile because the field is a `[*c]const u8`, assign `null` the same way; the field exists in `cimgui.h` line ~2656.

- [ ] **Step 5: Wire the build steps**

In `build.zig`, directly after the `editor-imgui` step from Task 3:

```zig
    const editor_imgui_module = b.createModule(.{
        .root_source_file = b.path("Sources/editor/imgui/imgui.zig"),
        .target = target,
        .optimize = optimize,
    });
    editor_imgui_module.addIncludePath(b.path("vendor/dcimgui/src-docking"));
    editor_imgui_module.addIncludePath(b.path("Sources/editor/imgui"));
    editor_imgui_module.linkLibrary(editor_imgui);

    const editor_overlay_spike_module = b.createModule(.{
        .root_source_file = b.path("tools/zig/editor_overlay_spike.zig"),
        .target = target,
        .optimize = optimize,
        .imports = &.{
            .{ .name = "sdl3", .module = sdl3 },
            .{ .name = "gfxgpu", .module = gfx_gpu_zig.root_module },
            .{ .name = "editor_imgui", .module = editor_imgui_module },
        },
    });
    const editor_overlay_spike = b.addExecutable(.{ .name = "editor-overlay-spike", .root_module = editor_overlay_spike_module });
    if (target.result.os.tag == .windows) editor_overlay_spike.subsystem = .console;
    const editor_overlay_spike_install = b.addInstallArtifact(editor_overlay_spike, .{});
    const editor_overlay_spike_build_step = b.step("editor-overlay-spike-build", "Build the Map Editor ImGui overlay spike");
    editor_overlay_spike_build_step.dependOn(&editor_overlay_spike_install.step);
```

The run step must come after `gfx_gpu_shaders_step` and `sdl_dynamic` exist (the smoke run near line 1580 uses both). Put it right after the `gfx_gpu_smoke_step` definition (line ~1599):

```zig
    const editor_overlay_spike_run = b.addRunArtifact(editor_overlay_spike);
    editor_overlay_spike_run.step.dependOn(&editor_overlay_spike_install.step);
    editor_overlay_spike_run.step.dependOn(gfx_gpu_shaders_step);
    editor_overlay_spike_run.step.dependOn(&b.addInstallArtifact(sdl_dynamic, .{ .dest_dir = .{ .override = .bin } }).step);
    editor_overlay_spike_run.setCwd(b.path("."));
    if (target.result.os.tag == .linux) editor_overlay_spike_run.setEnvironmentVariable("LD_LIBRARY_PATH", "zig-out/bin:zig-out/lib");
    if (b.args) |args| editor_overlay_spike_run.addArgs(args);
    const editor_overlay_spike_step = b.step("editor-overlay-spike", "Run the Map Editor ImGui overlay spike");
    editor_overlay_spike_step.dependOn(&editor_overlay_spike_run.step);
```

If the spike's definitions (Task 3's `editor_imgui` and this task's module) sit below line 1599 in `build.zig`, move them above it, next to `gfx_gpu_smoke_module`, so every name exists where it is used. `sdl3` and `gfx_gpu_zig` must be defined before the module; check with `grep -n "const sdl3 = \|const gfx_gpu_zig = \|const gfx_gpu_smoke_step" build.zig` and order accordingly.

- [ ] **Step 6: Build**

Run: `zig build editor-overlay-spike-build -Dtarget=aarch64-macos`
Expected: success, `zig-out/bin/editor-overlay-spike` exists. Link errors for `SDL_*` symbols mean the executable is not linking SDL3 through the `sdl3` module; in that case add `editor_imgui_module.linkLibrary(sdl_dynamic)` only if `sdl_dynamic` is the same library the `sdl3` module links (compare `otool -L zig-out/bin/gfxgpu-smoke` if it builds, or any other tool importing `sdl3`). Never link a second, static SDL3.

- [ ] **Step 7: Run it with a visible window**

```bash
mkdir -p zig-out/local-test/overlay-spike
zig build editor-overlay-spike -Dtarget=aarch64-macos -- --out zig-out/local-test/overlay-spike/visible_frame.rgba
```

Expected: a window flashes up and the step prints `overlay-spike: PASS format=... size=320x240 hidden=false inside=(255,0,255) outside=(0,255,0)` (sizes differ on a Retina-scaled window; values close to those). This is the spike's main result.

Look at the capture too, taking the size from the PASS line:

```bash
zig build editor-overlay-spike -Dtarget=aarch64-macos -- --out zig-out/local-test/overlay-spike/visible_frame.rgba 2>&1 | tee zig-out/local-test/overlay-spike/visible.log
W=$(grep -o 'size=[0-9]*x[0-9]*' zig-out/local-test/overlay-spike/visible.log | head -1 | cut -d= -f2)
ffmpeg -y -loglevel error -f rawvideo -pixel_format bgra -video_size "$W" -i zig-out/local-test/overlay-spike/visible_frame.rgba zig-out/local-test/overlay-spike/visible_frame.png
```

Then read `zig-out/local-test/overlay-spike/visible_frame.png`: a green frame with a magenta rectangle at the top left. If the colours look swapped in the PNG, the format is RGBA; use `-pixel_format rgba`.

- [ ] **Step 8: Run it with a hidden window and record what happens**

Run: `zig build editor-overlay-spike -Dtarget=aarch64-macos -- --hidden`
Expected: either PASS, or `FAIL no frame could be presented` (exit 2). Both are valid outcomes of the spike; the result decides how the engine test tier gets its window (spec, "Startup contract"). Write it down for Task 5.

- [ ] **Step 9: Commit**

```bash
git add Sources/editor/imgui tools/zig/editor_overlay_spike.zig build.zig
git commit -m "feat(editor): ImGui from Zig drawn over a GFXGPU frame, with a measured spike" -m "Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 5: CI build gate and the spike's findings in the spec

**Files:**
- Modify: `.github/workflows/cross-platform.yml` (one build step per job, next to each job's `test-gfxgpu-core` step: lines ~89, ~219, ~318, ~414, ~499, ~596)
- Modify: `docs/superpowers/specs/2026-09-19-portable-map-editor-design.md` (section "Drawing ImGui on top of the engine", and the "Engine" row of the test tier table)

**Interfaces:**
- Consumes: steps `editor-overlay-spike-build` (Task 4) and `test-gfxgpu-core` (unchanged, now with Task 1 and 2 tests).

- [ ] **Step 1: Add the build step to every CI job**

After each job's "Run GFX GPU effect table tests" step, add a step that builds the spike with exactly the same target and extra arguments as that job's `test-gfxgpu-core` line. For the Linux x64 job:

```yaml
      - name: Build the Map Editor overlay spike
        run: zig build editor-overlay-spike-build -Dtarget=x86_64-linux-gnu
```

For the Windows MSVC job, copy the `-D...` arguments of its `test-gfxgpu-core` line (line ~219) after `-Dtarget=x86_64-windows-msvc`. For the macOS jobs keep the `--sysroot "$MACOS_SYSROOT"` argument. Do not run the spike in CI: runners have no GPU device (spec, "Test tiers and the M1 CI gate").

- [ ] **Step 2: Build the Linux target locally as a cross-compile check**

Run: `zig build editor-overlay-spike-build -Dtarget=x86_64-linux-gnu`
Expected: success. If it fails in `addLinuxCxxIncludePaths` because this machine has no Linux sysroot, note it and rely on CI for Linux.

- [ ] **Step 3: Record the findings in the spec**

In the section "Drawing ImGui on top of the engine", replace the paragraph starting "This is the riskiest piece" with the result. **Done** - the measured values below are the ones now in the spec, re-confirmed 2026-09-21 after the capture texture moved to its own helper:

```markdown
Settled by the overlay spike (plan
`docs/superpowers/plans/2026-09-19-map-editor-01-overlay-spike.md`):

- The overlay callback runs in `endFrame` after the scene blit and before
  submit. It owns its own `LOAD` render pass on the target, because ImGui's
  SDL GPU backend uploads in a copy pass that may not overlap a render pass.
- ImGui draws at drawable resolution, independent of the scene size.
- A capture composes the frame into a readable texture and copies it to the
  swapchain, so tests read back exactly what was presented.
- Measured on macOS arm64 (`metal`, swapchain format `12`, frame size
  `320x240`): the panel pixel was `(255,0,255)`, the scene pixel `(0,255,0)`.
- Hidden window: `PASS` - same driver, format, size and pixels as the visible
  run, so the engine tier can use a hidden window as the spec assumes.
```

The hidden window passed, so the "Engine" row of the test tier table and the sentence under "Startup contract" stand as written: engine tests use a real, hidden SDL window. (Had it failed, both would have had to say a small visible window instead, as `gfxgpu-smoke` uses.)

- [ ] **Step 4: Commit**

```bash
git add .github/workflows/cross-platform.yml docs/superpowers/specs/2026-09-19-portable-map-editor-design.md
git commit -m "ci(editor): build the overlay spike on every platform; record the spike's findings" -m "Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

- [ ] **Step 5: Push the branch and watch CI (only after the user approves pushing)**

Ask the user before pushing. Once approved:

```bash
git push -u origin feat/portable-map-editor
gh run watch $(gh run list --branch feat/portable-map-editor --limit 1 --json databaseId --jq '.[0].databaseId') --exit-status
```

Expected: all six platform jobs succeed.

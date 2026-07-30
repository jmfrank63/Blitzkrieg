# P06-M02 — Adapt Lifecycle, Display, Frame, and Core State

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Delegate init/shutdown, mode, viewport, clear, begin/end/flip, transforms, material, lights, and render state.

**Dependencies:** P06-M01.

**Allowed files:** `Sources/src/GFXGPU/GraphicsEngineGpu.h`, `Sources/src/GFXGPU/GraphicsEngineGpu.cpp`, `Sources/src/GFXGPU/gfxgpu_c.h`, `Sources/src/GFXGPU/abi.zig`, `Sources/src/GFXGPU/renderer.zig`, `tools/zig/gfxgpu_factory_test.cpp`.

- [x] Build a recording fake `GfxGpuApi` in the C++ test and assert exact argument conversion and call order for each method group.
- [x] On the game/window thread, initialize the SDL video subsystem, create properties, set `SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER` to `GFXNativeWindow.value`, and call `SDL_CreateWindowWithProperties` to wrap the existing Windows window. The adapter owns the SDL wrapper but not the `HWND`; test that destroying the wrapper leaves the native window valid. Pass only the resulting `SDL_Window*` as the ABI's opaque `sdl_window`.
- [x] Map `Init`, mode/size queries, `SetViewport`, clear flags/color/depth/stencil, `BeginScene`, `EndScene`, and `Flip`.
- [x] Append/map world/view/projection, material, eight lights, fog, cull, depth, blend, alpha-test, stencil, and shade-effect operations.
- [x] Centralize result conversion in `Check(GfxGpuResult, operation)`; copy Zig diagnostic and use existing GFX logging/error conventions.
- [x] Run C++ recording tests and GPU smoke regression.
- [x] Commit: `feat: adapt IGFX lifecycle and render state`

**Evidence:** recording trace fixtures and mapped method inventory.

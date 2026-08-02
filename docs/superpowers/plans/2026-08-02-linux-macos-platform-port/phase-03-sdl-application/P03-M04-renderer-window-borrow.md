# P03-M04 — Make the Renderer Borrow the SDL Window

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Remove Win32 HWND adoption and SDL video/window ownership from `GraphicsEngineGpu`.

**Dependencies:** P03-M02.

**Allowed files:** `Sources/src/GFX/GFXPlatform.h`, `Sources/src/GFXGPU/GraphicsEngineGpu.h`, `Sources/src/GFXGPU/GraphicsEngineGpu.cpp`, `Sources/src/GFXGPU/gfxgpu_c.h`, `tools/zig/gfxgpu_factory_test.cpp`, `tools/zig/gfxgpu_smoke.zig`, `build.zig`.

- [ ] Extend factory/smoke tests with an SDL-owned window and verify renderer init/done never changes SDL video initialization or destroys the window.
- [ ] Pass the borrowed `SDL_Window *` directly in `GfxGpuCreateInfo.sdl_window`; delete `SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER`, window property creation, and adapter-owned SDL window/video flags.
- [ ] Keep the pointer opaque in `GFXNativeWindow` and `gfxgpu_c.h`; no SDL header enters the public GFX contract.
- [ ] Verify renderer destruction releases its GPU claim/device before the smoke owner destroys the window.
- [ ] Run Windows `gfxgpu-factory-test`, `gfxgpu-smoke`, and `test-gfxgpu`; compile Linux/macOS smoke artifacts.
- [ ] Commit: `gfx: borrow the SDL application window`

**Evidence:** ownership assertions and zero live renderer resources.

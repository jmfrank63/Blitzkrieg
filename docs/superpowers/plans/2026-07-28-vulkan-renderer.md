# Vulkan Renderer Implementation Plan

**Goal:** Migrate the Blitzkrieg game engine from DirectX 9 to Vulkan on Windows x64, then remove the DX9 backend entirely — using Zig for the new renderer core.

**Architecture:** A new DLL (`GFXVK.dll`) implements the existing `IGFX` interface behind a thin C++ bridge that delegates to a Zig Vulkan core via a plain C ABI (`gfxvk_c.h`). The manager layer (`TextureManager`, `FontManager`, `GeometryManager`, etc.) is backend-agnostic and reused. Only 4 C++ files that touch D3D directly are replaced: `GraphicsEngine`, `Texture`, `GeometryBuffer`, `VideoCheck` + `Specific.h`.

**Tech Stack:** Zig 0.16, vulkan-zig, Vulkan SDK 1.3+, glslc, zig-win32.

## Phases

- [ ] **Phase 0 — Scaffolding:** Branch, SDK, `build.zig.zon` + `vulkan-zig`, `-Drenderer`, `zig-win32`,  option, `addGFXVK` build target, shader compile step.
- [ ] **Phase 1 — Skeleton DLL:** `gfxvk_c.h` ABI, Zig stub, C++ bridge reusing manager layer, module descriptor. Game boots, renders nothing.
- [ ] **Phase 2 — Vulkan core windows:** instance/device/swapchain, validation layers, `BeginScene/EndScene/Clear/Flip`. Clear color on screen.
- [ ] **Phase 3 — 2D rendering (menus):** ring buffers, texture upload, 2D pipelines (effects 3/15/16), fonts. Screenshot A/B parity with DX9.
- [ ] **Phase 4 — 3D rendering (missions):** full effect catalog (39 IDs), transform UBOs, matrix-palette skinning, fixed-function lighting, terrain, shadows, stencil, particles, water.
- [ ] **Phase 5 — Feature parity + flip default:** render targets, screenshot, gamma post-process, performance pass. Default renderer becomes Vulkan.
- [ ] **Phase 6 — Remove DX9:** delete 4 D3D-touching files, drop d3d9/dxguid linkage, remove `addGFX` from build.zig.

## Files to Create

`Sources/src/GFXVK/GFXVK.def`, `Sources/src/GFXVK/gfxvk_c.h`, `Sources/src/GFXVK/gfxvk.zig`, `Sources/src/GFXVK/context.zig`, `Sources/src/GFXVK/bridge.cpp`, `Sources/src/GFXVK/GfxVkObjectFactory.cpp`, `Sources/src/GFXVK/GfxVkModuleChecker.cpp`, `Sources/src/GFXVK/shaders/ff.vert`, `Sources/src/GFXVK/shaders/ff.frag`, `build.zig.zon`.

## Test

`zig build test` (unit tests: FVF decoder, effect table). `zig build run -Drenderer=vulkan` (visual validation).

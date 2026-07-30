# P06-M06 — Complete Utility Methods and Link the Game

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Complete text/rectangle/screenshot/gamma/stat methods and make the full game link against the selectable adapter.

**Dependencies:** P06-M05 mesh adapter and draw integration.

**Allowed files:** `Sources/src/GFXGPU/GraphicsEngineGpu.cpp`, `Sources/src/GFXGPU/TextureGpu.cpp`, `Sources/src/GFXGPU/MeshGpu.h`, `Sources/src/GFXGPU/MeshGpu.cpp`, `Sources/src/GFXGPU/MeshManagerGpu.h`, `Sources/src/GFXGPU/MeshManagerGpu.cpp`, `Sources/src/GFXGPU/GfxGpuObjectFactory.cpp`, `build.zig`, `tools/zig/gfxgpu_factory_test.cpp`.

- [x] Inventory remaining pure virtual and utility methods after P06-M05; diagnostics now identify unsupported font-resource and screenshot paths explicitly.
- [x] Register the renderer-neutral mesh manager/resource path used by the game, without linking `GeometryMesh.cpp` or other legacy DirectX implementation objects into `GFXGPU.dll`.
- [x] Route rectangles and fallback text glyph quads through temporary geometry and untextured effects.
- [x] Add a standalone readback ABI export and `TakeScreenShot`/`IImage` row-copy consumer without changing file formats. The export currently returns explicit unsupported until the renderer owns an SDL swapchain texture/command buffer; that backend is the remaining implementation item.
- [x] Preserve gamma API shape; return an explicit unsupported result when hardware gamma is unavailable while retaining correction values for callers.
- [x] Map statistics/live counts and display-mode queries needed by current Windows startup.
- [x] Build `gfx`, `game`, and `game-all` with `-Drenderer=sdl_gpu`; inspect link inputs for `d3d9`, `dxguid`, and legacy `GraphicsEngine.cpp`.
- [ ] Commit: `feat: add SDL GPU screenshot readback seam` after clean ABI and game verification.

**Evidence:** successful factory/core/renderer/game builds; `Game.exe` imports contain no `d3d9`, `dxguid`, or legacy `GraphicsEngine` symbols. The standalone readback ABI and C++ image copy path are present; actual SDL GPU swapchain readback remains to be wired into the renderer frame backend.

# P06-M06 — Complete Utility Methods and Link the Game

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Complete text/rectangle/screenshot/gamma/stat methods and make the full game link against the selectable adapter.

**Dependencies:** P06-M05 mesh adapter and draw integration.

**Allowed files:** `Sources/src/GFXGPU/GraphicsEngineGpu.cpp`, `Sources/src/GFXGPU/TextureGpu.cpp`, `build.zig`, `tools/zig/gfxgpu_adapter_test.cpp`.

- [ ] Inventory remaining pure virtual and utility methods after P06-M05; the test fails if any adapter diagnostic says unimplemented.
- [ ] Register the renderer-neutral mesh manager/resource path used by the game, without linking `GeometryMesh.cpp` or other legacy DirectX implementation objects into `GFXGPU.dll`.
- [ ] Route rectangles and text glyph quads through temporary geometry and textured/untextured effects.
- [ ] Route screenshots through readback and existing image-save code without changing file formats.
- [ ] Preserve gamma API shape; return an explicit unsupported result when hardware gamma is unavailable and ensure callers follow the legacy non-fatal path.
- [ ] Map statistics/live counts and display-mode queries needed by current Windows startup.
- [ ] Build `gfx`, `game`, and `game-all` with `-Drenderer=sdl_gpu`; inspect link inputs for `d3d9`, `dxguid`, and legacy `GraphicsEngine.cpp`.
- [ ] Commit: `feat: link game with SDL GPU adapter`

**Evidence:** zero unimplemented methods, successful game link, and renderer-specific link-input report.

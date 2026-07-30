# P07-M02 — Implement UI, Text, Unlit, and Alpha-Test Effects

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Cover 2D rectangles/glyphs and core single-texture 3D paths.

**Dependencies:** P07-M01.

**Allowed files:** `Sources/src/GFXGPU/shaders/manifest.json`, `Sources/src/GFXGPU/shaders/ui.hlsl`, `Sources/src/GFXGPU/shaders/unlit.hlsl`, `Sources/src/GFXGPU/shaders/common.hlsl`, `Sources/src/GFXGPU/effects.zig`, `tools/zig/gfxgpu_smoke.cpp`.

- [ ] Add CPU reference cases for half-pixel/UI projection policy, glyph alpha modulation, vertex color, one-texture modulation, and all alpha-test compare functions used by inventory.
- [ ] Implement UI/text variants with depth disabled and catalog-defined blending.
- [ ] Implement unlit opaque with optional vertex color and texture.
- [ ] Implement alpha test with shader `clip`; preserve the legacy alpha reference conversion from 0–255 to normalized float.
- [ ] Add manifest records and ensure declared attributes/resources match each catalog spec.
- [ ] Extend probe scene with UI/text alpha edges and cutout geometry.
- [ ] Commit: `feat: add UI and unlit shader effects`

**Evidence:** edge probe pixels and effect IDs covered.

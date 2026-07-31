# P07-M04 — Implement Multitexture, Lightmaps, and Fixed Lighting

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Match two-texture combine modes and the legacy material/light model.

**Dependencies:** P07-M03.

**Allowed files:** `Sources/src/GFXGPU/shaders/manifest.json`, `Sources/src/GFXGPU/shaders/lightmap.hlsl`, `Sources/src/GFXGPU/shaders/lighting.hlsl`, `Sources/src/GFXGPU/shaders/common.hlsl`, `Sources/src/GFXGPU/effects.zig`, `Sources/src/GFXGPU/bindings.zig`, `tools/zig/gfxgpu_smoke.cpp`.

- [x] Inventory texture-stage operations used for stages 0/1 and legacy light types/fields.
- [x] Add CPU fixtures for modulate/add/signed-add operations actually used, UV0/UV1 selection, ambient/diffuse/specular/emissive material, directional/point lights, attenuation, and enabled-light count.
- [x] Implement separate lightmap and lit shader pairs with fixed two-sampler maximum and eight-light uniform maximum.
- [x] Transform normals with the selected legacy-compatible matrix policy and normalize before lighting.
- [x] Define behavior for singular transforms as deterministic zero contribution plus diagnostic counter, not a crash.
- [x] Add textured quad and lit sphere/cube probes.
- [x] Commit: `feat: add lightmap and fixed lighting effects`

**Evidence:** stage-op/light inventory, CPU vectors, visual probes.

# P07-M05 — Complete Stencil, Shadow, Water, and Special Effects

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Implement all remaining inventoried effect paths without fallback substitution.

**Dependencies:** P07-M04.

**Allowed files:** `Sources/src/GFXGPU/shaders/manifest.json`, `Sources/src/GFXGPU/shaders/stencil_shadow.hlsl`, `Sources/src/GFXGPU/shaders/water.hlsl`, `Sources/src/GFXGPU/shaders/special.hlsl`, `Sources/src/GFXGPU/shaders/common.hlsl`, `Sources/src/GFXGPU/effects.zig`, `tools/zig/gfxgpu_smoke.cpp`.

- [x] Convert each remaining inventory row into a failing catalog/CPU/pipeline test before adding implementation.
- [x] Implement stencil write/test and shadow blend/depth policies through pipeline keys, with shader color logic only where required.
- [x] Implement water UV animation/combine and each special effect from its observed legacy formulas and bound inputs.
- [x] Never route an unimplemented effect to unlit/textured fallback. Unknown or incomplete effects fail with ID/name.
- [x] Add probe geometry for stencil mask/inside/outside, projected shadow, water animation at fixed time, and each special family.
- [x] Run shader compile, pipeline creation for every effect, and GPU probes three times.
- [x] Commit: `feat: complete legacy GPU effect catalog`

**Evidence:** zero uncovered inventory rows, effect pipeline count, probe hashes.

# P07-M03 — Implement Alpha Blend, Particles, and Fog

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Match transparent geometry, particle color/texture behavior, and legacy fog.

**Dependencies:** P07-M02.

**Allowed files:** `Sources/src/GFXGPU/shaders/manifest.json`, `Sources/src/GFXGPU/shaders/transparent.hlsl`, `Sources/src/GFXGPU/shaders/particle.hlsl`, `Sources/src/GFXGPU/shaders/common.hlsl`, `Sources/src/GFXGPU/effects.zig`, `tools/zig/gfxgpu_smoke.cpp`.

- [x] Add blend-equation CPU fixtures for every source/destination factor and operation present in the inventory.
- [x] Implement straight-alpha paths unless the inventory proves a specific premultiplied path.
- [x] Implement particle billboard/point expansion exactly where legacy CPU geometry does not already expand it; do not expand twice.
- [x] Implement linear/range fog using the same coordinate and clamp policy as legacy state.
- [x] Add overlapping transparent and near/far fog probes with tolerance ranges.
- [x] Require catalog pipeline keys to encode blend and depth-write policy.
- [x] Commit: `feat: add transparent particle and fog effects`

**Evidence:** blend/fog fixtures and probe pixel values.

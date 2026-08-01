# P01-M02 — Map Legacy Formats and Primitive Types

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Convert every used legacy pixel, index, primitive, comparison, cull, blend, and clear value into backend-neutral Zig enums.

**Dependencies:** P01-M01.

**Allowed files:** `Sources/src/GFXGPU/formats.zig`, `Sources/src/GFXGPU/root.zig`.

- [ ] Copy numeric constants from `Sources/src/GFX/GFXTypes.h` into test-only tables; do not import D3D headers.
- [ ] Add exhaustive table tests for DXT1/3/5, A8/R8/RG8/RGBA8/BGRA8 depth formats actually used by assets, 16/32-bit indices, point/line/triangle topology, comparison, cull, blend factors/ops, and clear masks.
- [ ] Define explicit renderer enums and `fromLegacy*` conversion functions returning `error.UnsupportedValue`.
- [ ] Add `primitiveVertexCount(topology, primitive_count)` with overflow checks and strip/list behavior.
- [ ] Verify every unknown integer is rejected and diagnostic formatting includes category plus value.
- [ ] Commit: `feat: normalize legacy graphics formats`

**Evidence:** table row counts and unsupported-value test output.

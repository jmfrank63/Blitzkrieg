# P01-M04 — Normalize State and Build Pipeline Keys

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Represent D3D-style mutable calls as normalized state and deterministic immutable pipeline keys.

**Dependencies:** P01-M03.

**Allowed files:** `Sources/src/GFXGPU/render_state.zig`, `Sources/src/GFXGPU/pipeline_key.zig`, `Sources/src/GFXGPU/root.zig`.

**Required key fields:** effect, vertex-layout digest, topology, color/depth formats, sample count, cull/fill, depth test/write/compare, stencil masks/ops, blend enable/factors/ops/write mask, alpha-test function, and attachment count.

- [ ] Add tests for documented defaults matching legacy startup state.
- [ ] Add mutation tests proving dynamic values such as matrices, viewport, texture handles, fog distances, and blend constant do not alter `PipelineKey`.
- [ ] Add key tests proving every immutable pipeline field changes equality and hash.
- [ ] Normalize disabled depth/blend/stencil fields so semantically equal states produce equal bytes.
- [ ] Use explicit field hashing; do not hash struct padding.
- [ ] Add `dirty.pipeline`, `dirty.bindings`, and `dirty.uniforms` flags with mutation-specific tests.
- [ ] Commit: `feat: model render state and pipeline keys`

**Evidence:** default-state fixture and key sensitivity test list.

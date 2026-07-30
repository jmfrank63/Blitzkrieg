# P03-M04 — Cache Immutable Samplers

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Normalize legacy sampler state and reuse immutable SDL GPU samplers.

**Dependencies:** P03-M03, P01-M04.

**Allowed files:** `Sources/src/GFXGPU/samplers.zig`, `Sources/src/GFXGPU/render_state.zig`, `Sources/src/GFXGPU/renderer.zig`, `Sources/src/GFXGPU/root.zig`.

**Sampler key:** min/mag/mip filters, U/V/W address modes, anisotropy, compare enable/function, min/max LOD, and normalized border policy.

- [ ] Add default/nearest/linear/aniso/clamp/wrap/mirror key tests from legacy sampler state.
- [ ] Prove disabled fields normalize to identical key bytes and hashes.
- [ ] Create one native sampler per unique key; reference it from the cache and destroy all entries during renderer shutdown.
- [ ] Cap anisotropy to queried device capability and record the effective value.
- [ ] Add cache hit/miss/live counts.
- [ ] Commit: `feat: cache immutable GPU samplers`

**Evidence:** cache reuse and normalized-key tests.

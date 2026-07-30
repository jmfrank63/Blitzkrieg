# P04-M04 — Fix Shared Shader Bindings and Uniform Layouts

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Define one byte-exact HLSL/Zig binding and uniform contract.

**Dependencies:** P04-M03, P01-M03.

**Allowed files:** `Sources/src/GFXGPU/shaders/bindings.hlsl`, `Sources/src/GFXGPU/shaders/common.hlsl`, `Sources/src/GFXGPU/bindings.zig`, `Sources/src/GFXGPU/root.zig`.

**Layouts:** `FrameUniforms` 80 bytes, `DrawUniforms` 80 bytes, `LightUniforms` fixed 512 bytes; all fields are 16-byte aligned.

- [ ] Define explicit HLSL fields and matching `extern struct` Zig values; add compile-time size/alignment/offset assertions.
- [ ] Add serialization golden tests for identity matrices, packed colors, fog disabled/enabled, and zero through eight lights.
- [ ] Implement bounded uniform push helpers for vertex slot 0/1/2 and fragment slot 0/1/2 as required by each shader record.
- [ ] Define attribute locations for position 0, weights 1, indices 2, normal 3, point size 4, diffuse 5, specular 6, UV0–UV7 at 7–14.
- [ ] Reject a shader record whose declared resource counts exceed the fixed convention.
- [ ] Commit: `feat: define GPU shader binding contract`

**Evidence:** compile-time layout assertions and serialized byte fixtures.

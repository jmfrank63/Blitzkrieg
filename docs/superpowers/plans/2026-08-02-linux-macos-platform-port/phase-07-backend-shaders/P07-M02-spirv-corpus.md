# P07-M02 — Generate and Validate the SPIR-V Shader Corpus

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Produce Linux/Vulkan SPIR-V artifacts for every accepted effect.

**Dependencies:** P07-M01.

**Allowed files:** `tools/zig/compile_gfxgpu_shaders.zig`, `Sources/src/GFXGPU/shaders/*.hlsl`, `Sources/src/GFXGPU/shaders/bindings.hlsl`, `Sources/src/GFXGPU/shaders/common.hlsl`, `build.zig`.

- [ ] Add `spirv` to the compiler format set and invoke the Zig-built shadercross artifact with source HLSL, destination SPIR-V, stage, entry, include root, and sorted defines.
- [ ] Validate SPIR-V magic/alignment/non-empty size plus reflected binding counts against each logical record.
- [ ] Add deterministic two-directory comparison for all `.spv` files and the combined manifest.
- [ ] Change HLSL only when shadercross proves a backend semantic incompatibility; add a focused shader parser/reflection test for each change and preserve DXIL output semantics.
- [ ] Run `gfxgpu-shaders -Dshader-formats=dxil,spirv` and determinism tests twice.
- [ ] Commit: `gfx: add deterministic SPIR-V shader assets`

**Evidence:** complete effect/stage SPIR-V list and hashes.

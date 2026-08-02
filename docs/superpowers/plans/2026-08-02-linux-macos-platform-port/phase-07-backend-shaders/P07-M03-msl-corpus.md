# P07-M03 — Generate and Validate the MSL Shader Corpus

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Produce Apple-Silicon/Metal MSL source artifacts for every accepted effect.

**Dependencies:** P07-M01, P07-M02.

**Allowed files:** `tools/zig/compile_gfxgpu_shaders.zig`, `Sources/src/GFXGPU/shaders/*.hlsl`, `Sources/src/GFXGPU/shaders/bindings.hlsl`, `Sources/src/GFXGPU/shaders/common.hlsl`, `build.zig`.

- [ ] Add MSL generation through the same Zig-built shadercross artifact and pin the MSL language version in the build graph.
- [ ] Validate UTF-8 text, required entry point, stage marker, non-empty source, reflected binding counts, and line-ending normalization before hashing.
- [ ] Add deterministic comparison for all `.msl` files and the combined manifest.
- [ ] Keep resource index mapping consistent with the binding convention; reject a reflected index/count mismatch.
- [ ] Run all-format generation/determinism on Windows and macOS hosts and compare committed evidence hashes, not generated blobs.
- [ ] Commit: `gfx: add deterministic MSL shader assets`

**Evidence:** complete MSL effect/stage hashes and binding validation.

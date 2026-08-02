# P07-M01 — Extend the Shader Manifest to Multiple Formats

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Represent DXIL, SPIR-V, and MSL records for every logical effect/stage without duplicating source metadata.

**Dependencies:** P06-M05.

**Allowed files:** `Sources/src/GFXGPU/shader_manifest.zig`, `Sources/src/GFXGPU/shaders/manifest.json`, `tools/zig/compile_gfxgpu_shaders.zig`, `build.zig`.

- [ ] Bump binary schema to version 3 and add fixed format values `dxil=1`, `spirv=2`, `msl=3`.
- [ ] Test duplicate `(effect,stage,format)`, missing required format, mismatched stage pairs, traversal/extension mismatch, malformed hash/length, deterministic sort by effect/stage/format, and v2 rejection with a clear version error.
- [ ] Keep JSON records logical—source, entry, stage, defines, masks, bindings—and generate format records from the build-requested format set.
- [ ] Require complete vertex/fragment pairs per format for every effect.
- [ ] Run parser tests and existing DXIL corpus generation without changing DXIL bytes.
- [ ] Commit: `gfx: define multi format shader manifest`

**Evidence:** schema fixture tests and unchanged DXIL hashes.

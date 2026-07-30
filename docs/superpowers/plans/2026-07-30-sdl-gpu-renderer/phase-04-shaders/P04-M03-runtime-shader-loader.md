# P04-M03 — Load and Own Runtime Shaders

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Validate generated manifests/blobs and create cached SDL_GPU shader pairs.

**Dependencies:** P04-M02, P02-M01.

**Allowed files:** `Sources/src/GFXGPU/shader_manifest.zig`, `Sources/src/GFXGPU/shaders.zig`, `Sources/src/GFXGPU/renderer.zig`, `Sources/src/GFXGPU/root.zig`.

- [ ] Add in-memory tests for bad magic/version/format, duplicate records, truncated fields, path traversal, length/hash mismatch, absent pair, and allocation/SDL creation rollback.
- [ ] Parse with explicit little-endian reads and checked lengths; never cast untrusted bytes to a struct.
- [ ] Require the manifest format to match the device's selected shader format.
- [ ] Create `SDL_GPUShader` with exact stage, entry point, sampler/storage/uniform counts, and bytecode.
- [ ] Cache by effect ID and release fragment before vertex entries during shutdown.
- [ ] Extend live counts for shaders and run malformed-input tests.
- [ ] Commit: `feat: load SDL GPU shader manifests`

**Evidence:** malformed corpus names and complete rollback counts.

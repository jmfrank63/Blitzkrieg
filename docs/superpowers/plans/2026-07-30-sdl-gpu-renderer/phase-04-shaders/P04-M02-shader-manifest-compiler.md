# P04-M02 — Define the Shader Manifest and Compiler Driver

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Turn explicit source records into deterministic backend blobs and a generated runtime manifest.

**Dependencies:** P04-M01.

**Allowed files:** `Sources/src/GFXGPU/shaders/manifest.json`, `Sources/src/GFXGPU/shaders/probe.hlsl`, `tools/zig/compile_gfxgpu_shaders.zig`, `build.zig`.

**Source record fields:** effect ID/name, stage, source path, entry point, defines, required vertex mask, sampler count, storage-texture count, storage-buffer count, uniform-buffer count.

- [ ] Add parser tests for one valid probe and failures for duplicate effect/stage, unknown stage, missing source, absolute path, path traversal, duplicate define, and missing entry point.
- [ ] Implement sorted record processing and output names `<effect>.<stage>.<format>`.
- [ ] Invoke shadercross with explicit `-s HLSL -d DXIL -t <stage> -e <entry> -I <shader-dir> -o <output>`.
- [ ] Generate a compact binary `gfxgpu-shaders.manifest` containing magic, schema version, format, records, relative blob paths, byte lengths, and content hashes.
- [ ] Add `gfxgpu-shaders` build step and a determinism test comparing hashes from two clean output directories.
- [ ] Commit: `feat: add deterministic GPU shader compiler`

**Evidence:** compiler command line, generated record dump, equal hashes.

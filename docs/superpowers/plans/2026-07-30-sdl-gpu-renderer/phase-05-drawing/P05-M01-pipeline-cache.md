# P05-M01 — Create and Cache Graphics Pipelines

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Translate `PipelineKey` and shader metadata into immutable SDL_GPU graphics pipelines.

**Dependencies:** P01-M04, P03-M05, P04-M05.

**Allowed files:** `Sources/src/GFXGPU/pipeline_cache.zig`, `Sources/src/GFXGPU/renderer.zig`, `Sources/src/GFXGPU/root.zig`.

- [ ] Add injected-API tests for cache hit, each key-field miss, shader absence, unsupported format/state, SDL creation failure, rollback, and release.
- [ ] Validate shader-required attributes against decoded vertex layout before creation.
- [ ] Populate vertex input, primitive topology, rasterizer, multisample, depth/stencil, color target format, and blend state explicitly.
- [ ] Use a deterministic hash map keyed by complete normalized key; equality resolves hash collisions.
- [ ] Mark returned pipeline's last-use serial and add hits/misses/live entries to statistics.
- [ ] Commit: `feat: cache SDL GPU graphics pipelines`

**Evidence:** key-field cache matrix and rollback live counts.

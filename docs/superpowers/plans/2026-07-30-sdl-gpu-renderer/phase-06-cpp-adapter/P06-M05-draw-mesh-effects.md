# P06-M05 — Adapt Draw, Mesh, and Effect Calls

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Route all engine draw entry points through the normalized Zig draw API and add a renderer-neutral mesh batch adapter.

**Dependencies:** P06-M04.

**Allowed files:** `Sources/src/GFXGPU/GraphicsEngineGpu.cpp`, `Sources/src/GFXGPU/GeometryBufferGpu.cpp`, `Sources/src/GFXGPU/MeshGpu.h`, `Sources/src/GFXGPU/MeshGpu.cpp`, `Sources/src/GFXGPU/GfxGpuObjectFactory.cpp`, `Sources/src/GFX/GFX.H`, `tools/zig/gfxgpu_factory_test.cpp`, and `build.zig`.

- [x] Inventory every draw/mesh/effect method in `IGFX` and its use in `Sources/src`; classify indexed, non-indexed, user-memory, buffered, and mesh paths.
- [x] Add recording tests for primitive/count conversion, base vertex, first index, FVF, buffer handles, user-memory byte sizes, and current effect.
- [x] Delegate buffered calls directly. Delegate user-memory calls through temporary geometry operations.
- [x] Define the renderer-neutral mesh batch contract as `MeshGpu::Part`/`Parts()` without changing the existing pure-virtual `IGFXMesh` layout; the adapter owns the concrete batch representation.
- [x] Implement `MeshGpu` loading from the existing `fmtMesh` asset representation, creating only `VerticesGpu`/`IndicesGpu` resources.
- [x] Translate mesh batches without retaining borrowed engine pointers after the call; set each part's matrix and submit its buffers immediately.
- [x] Validate null/count/range conditions in C++ only when required by the virtual-interface contract; leave renderer resource validation to Zig.
- [x] Require each inventoried method to have one recording assertion.
- [x] Add factory-harness coverage for `MeshGpu::Build` and `DrawMesh` matrix/buffer submission.
- [x] Commit: `feat: adapt IGFX draw and mesh submission` (draw/effect portion), followed by the renderer-neutral mesh adapter implementation.

**Evidence:** complete method/use classification, recording tests, and passing `gfxgpu-factory-test`, `test-gfxgpu-core`, `gfx-sdl-gpu`, `gfx`, and `gfx-legacy` Windows x64 builds.

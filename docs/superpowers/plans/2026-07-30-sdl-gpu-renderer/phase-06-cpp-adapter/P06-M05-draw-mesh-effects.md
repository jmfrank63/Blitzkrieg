# P06-M05 — Adapt Draw, Mesh, and Effect Calls

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Route all engine draw entry points through the normalized Zig draw API and add a renderer-neutral mesh batch adapter.

**Dependencies:** P06-M04.

**Allowed files:** `Sources/src/GFXGPU/GraphicsEngineGpu.cpp`, `Sources/src/GFXGPU/GeometryBufferGpu.cpp`, `Sources/src/GFXGPU/MeshGpu.h`, `Sources/src/GFXGPU/MeshGpu.cpp`, `Sources/src/GFXGPU/GfxGpuObjectFactory.cpp`, `Sources/src/GFX/GFX.H`, `tools/zig/gfxgpu_adapter_test.cpp`.

- [x] Inventory every draw/mesh/effect method in `IGFX` and its use in `Sources/src`; classify indexed, non-indexed, user-memory, buffered, and mesh paths.
- [x] Add recording tests for primitive/count conversion, base vertex, first index, FVF, buffer handles, user-memory byte sizes, and current effect.
- [x] Delegate buffered calls directly. Delegate user-memory calls through temporary geometry operations.
- [ ] Define a renderer-neutral `IGFXMeshBatch` query contract without changing the existing pure-virtual `IGFXMesh` layout.
- [ ] Implement `MeshGpu` loading from the existing `fmtMesh` asset representation, creating only `VerticesGpu`/`IndicesGpu` resources.
- [ ] Translate mesh batches without retaining borrowed engine pointers after the call; set each part's matrix and submit its buffers immediately.
- [x] Validate null/count/range conditions in C++ only when required by the virtual-interface contract; leave renderer resource validation to Zig.
- [x] Require each inventoried method to have one recording assertion.
- [x] Commit: `feat: adapt IGFX draw and mesh submission` (draw/effect portion)

**Evidence:** complete method/use classification and recording tests.

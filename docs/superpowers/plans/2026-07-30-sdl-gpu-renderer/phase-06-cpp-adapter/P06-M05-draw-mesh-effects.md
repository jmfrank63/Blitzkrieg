# P06-M05 — Adapt Draw, Mesh, and Effect Calls

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Route all engine draw entry points through the normalized Zig draw API.

**Dependencies:** P06-M04.

**Allowed files:** `Sources/src/GFXGPU/GraphicsEngineGpu.cpp`, `Sources/src/GFXGPU/GeometryBufferGpu.cpp`, `tools/zig/gfxgpu_adapter_test.cpp`.

- [ ] Inventory every draw/mesh/effect method in `IGFX` and its use in `Sources/src`; classify indexed, non-indexed, user-memory, buffered, and mesh paths.
- [ ] Add recording tests for primitive/count conversion, base vertex, first index, FVF, buffer handles, user-memory byte sizes, and current effect.
- [ ] Delegate buffered calls directly. Delegate user-memory calls through temporary geometry operations.
- [ ] Translate mesh batches without retaining borrowed engine pointers after the call.
- [ ] Validate null/count/range conditions in C++ only when required by the virtual-interface contract; leave renderer resource validation to Zig.
- [ ] Require each inventoried method to have one recording assertion.
- [ ] Commit: `feat: adapt IGFX draw and mesh submission`

**Evidence:** complete method/use classification and recording tests.

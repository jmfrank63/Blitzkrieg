# P06-M04 — Adapt Vertex, Index, and Temporary Geometry

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Implement legacy buffer interfaces with Zig handles and bounded CPU lock staging.

**Dependencies:** P06-M03, P03-M02.

**Allowed files:** `Sources/src/GFXGPU/GeometryBufferGpu.h`, `Sources/src/GFXGPU/GeometryBufferGpu.cpp`, `Sources/src/GFXGPU/GraphicsEngineGpu.cpp`, `tools/zig/gfxgpu_adapter_test.cpp`.

- [ ] Inventory vertex/index/geometry virtual methods and all lock flag combinations used by source call sites.
- [ ] Test static creation, dynamic discard/no-overwrite policy mapping, partial lock bounds, double lock, unlock upload, 16/32-bit indices, refcount/release, and temporary geometry.
- [ ] Store handle, byte size, stride/FVF or index format, dynamic flag, current staging allocation, lock range, and refcount.
- [ ] Convert FVF only in Zig; C++ passes the original fixed-width mask and stride.
- [ ] Guarantee staging allocation is freed after both successful and failed unlock.
- [ ] Use immediate ABI temporary upload/draw paths for legacy temporary buffers; no persistent C++ GPU object.
- [ ] Commit: `feat: adapt IGFX geometry buffers`

**Evidence:** call-site lock inventory and allocation-balance tests.

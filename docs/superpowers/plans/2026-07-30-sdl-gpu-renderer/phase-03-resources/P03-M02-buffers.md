# P03-M02 — Implement Static, Dynamic, and Temporary Buffers

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Create/upload/destroy vertex and index buffers and support frame-cycled dynamic geometry.

**Dependencies:** P03-M01, P01-M03.

**Allowed files:** `Sources/src/GFXGPU/buffers.zig`, `Sources/src/GFXGPU/renderer.zig`, `Sources/src/GFXGPU/abi.zig`, `Sources/src/GFXGPU/gfxgpu_c.h`, `Sources/src/GFXGPU/root.zig`.

**ABI operations:** create/destroy/upload vertex buffer; create/destroy/upload index buffer; allocate temporary vertex/index ranges.

- [ ] Test usage flags, size overflow, initial upload, partial update bounds, index alignment, stale handle, and rollback after each injected SDL failure.
- [ ] Store size, usage, stride/index format, native pointer, and last-use serial in each registry entry.
- [ ] Static buffers reject updates unless created dynamic. Dynamic buffers use transfer copies; never map GPU buffers.
- [ ] Implement a three-frame temporary-buffer ring that grows geometrically and never overwrites in-flight data.
- [ ] Add C ABI structs with `struct_size`, byte count, stride/format, dynamic flag, and borrowed data pointer.
- [ ] Run core, ABI, and resource smoke tests.
- [ ] Commit: `feat: own GPU geometry buffers`

**Evidence:** bounds/rollback tests and temporary-ring high-water mark.

# P01-M01 — Harden Errors and Generational Handles

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Add deterministic diagnostic storage and typed index-plus-generation registries.

**Dependencies:** P00-M04.

**Allowed files:** `Sources/src/GFXGPU/error.zig`, `Sources/src/GFXGPU/handles.zig`, `Sources/src/GFXGPU/root.zig`.

**Required operations:** `insert`, `get`, `getMut`, `remove`, `liveCount`, `deinit(assert_empty)`.

- [ ] Test first: zero handle rejection; insert/get; removal increments generation; stale handle rejection; slot reuse; generation wrap skips zero; wrong registry rejection; all allocation failures leave counts unchanged.
- [ ] Encode low 32 bits as slot index plus one and high 32 bits as non-zero generation.
- [ ] Implement `Registry(comptime T: type)` with an allocator-owned slot array and free-index stack.
- [ ] Store diagnostics in a fixed 1024-byte renderer-owned buffer with truncation and explicit NUL termination at the ABI copy boundary.
- [ ] Run `test-gfxgpu-core` under Debug and ReleaseSafe.
- [ ] Commit: `feat: add renderer errors and generational handles`

**Evidence:** stale-handle and allocation-failure test names.

# P05-M02 — Control Render-Pass Lifetime

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Begin/end attachment-compatible render passes with correct load/store operations.

**Dependencies:** P05-M01, P02-M03.

**Allowed files:** `Sources/src/GFXGPU/passes.zig`, `Sources/src/GFXGPU/frame.zig`, `Sources/src/GFXGPU/renderer.zig`, `Sources/src/GFXGPU/root.zig`.

- [x] Add a pure `PassPlan` test matrix for first use, clear color/depth/stencil, preserve, target change, frame end, and skipped swapchain.
- [x] Begin lazily on clear or first draw. Consume pending clear values exactly once.
- [x] Reuse the active pass while attachments/sample count remain equal.
- [x] End before target switch, copy pass, readback, frame end, or cancellation.
- [x] On failure, leave the frame state cancellable and preserve no dangling active-pass pointer.
- [ ] Commit: `feat: manage SDL GPU render passes`

**Evidence:** PassPlan matrix and failure-state tests.

# P02-M03 — Implement the Frame State Machine

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Enforce valid command-buffer, swapchain, pass, submit, and cancel transitions.

**Dependencies:** P02-M02.

**Allowed files:** `Sources/src/GFXGPU/frame.zig`, `Sources/src/GFXGPU/renderer.zig`, `Sources/src/GFXGPU/abi.zig`, `Sources/src/GFXGPU/gfxgpu_c.h`, `Sources/src/GFXGPU/root.zig`.

**States:** `idle`, `recording`, `pass_active`, `ready_to_submit`.

- [ ] Add table-driven tests for every valid transition and invalid operation from each state.
- [ ] Append ABI operations `begin_frame`, `end_frame`, `present`, and `cancel_frame`.
- [ ] `begin_frame` acquires one command buffer then one swapchain texture. A null acquired texture is a non-fatal skipped frame.
- [ ] `end_frame` closes any active pass through the pass owner introduced later; until then it requires no active pass.
- [ ] `present` submits exactly once and returns to idle. On error, release/cancel according to SDL's ownership contract and return to a reusable state.
- [ ] Run pure state tests and the C++ ABI table-size test.
- [ ] Commit: `feat: enforce GPU frame lifecycle`

**Evidence:** transition table and skipped-frame behavior.

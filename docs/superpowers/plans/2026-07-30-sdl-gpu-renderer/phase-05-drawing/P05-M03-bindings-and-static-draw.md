# P05-M03 — Bind State and Draw Static Geometry

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Bind pipeline, viewport/scissor, buffers, textures/samplers, uniforms, and issue non-indexed static draws.

**Dependencies:** P05-M02, P03-M02, P03-M04.

**Allowed files:** `Sources/src/GFXGPU/bindings.zig`, `Sources/src/GFXGPU/draw.zig`, `Sources/src/GFXGPU/renderer.zig`, `Sources/src/GFXGPU/abi.zig`, `Sources/src/GFXGPU/gfxgpu_c.h`, `Sources/src/GFXGPU/root.zig`.

- [ ] Append ABI setters for viewport, transforms, color/fog/effect, texture/sampler state and `draw`.
- [ ] Test missing frame/buffer/shader, out-of-range vertices, topology count overflow, render-target feedback, dirty-flag clearing, redundant bind suppression, and last-use serial updates.
- [ ] Build a `DrawPlan` before touching SDL; validation failure emits no SDL call.
- [ ] Apply pipeline first, then viewport/scissor, vertex buffers, vertex/fragment samplers, uniforms, and draw.
- [ ] Preserve dirty bits after a failed bind; clear only successfully encoded groups.
- [ ] Add untextured static triangle smoke/readback.
- [ ] Commit: `feat: submit static SDL GPU draws`

**Evidence:** exact call-order fake trace and triangle pixels.

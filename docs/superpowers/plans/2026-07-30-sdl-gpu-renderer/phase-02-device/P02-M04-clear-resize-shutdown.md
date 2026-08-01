# P02-M04 — Clear, Present, Resize, and Restart

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Render deterministic clear frames and survive drawable-size changes, minimize/restore, and renderer restart.

**Dependencies:** P02-M03.

**Allowed files:** `Sources/src/GFXGPU/frame.zig`, `Sources/src/GFXGPU/surface.zig`, `Sources/src/GFXGPU/renderer.zig`, `Sources/src/GFXGPU/abi.zig`, `Sources/src/GFXGPU/gfxgpu_c.h`, `tools/zig/gfxgpu_smoke.cpp`.

- [ ] Append ABI `clear` and `resize` declarations with color/depth/stencil mask and values.
- [ ] Add fake tests proving clear uses LOAD when a mask is absent and CLEAR for selected attachments, resize is coalesced before the next frame, and zero drawable size skips acquisition.
- [ ] Record a color-only render pass against the swapchain, end it, and submit.
- [ ] Make smoke render 40 red, 40 green, and 40 blue frames; resize after frame 40, hide/show after frame 80, and recreate the renderer once before exit.
- [ ] Run smoke three times with D3D12 forced and SDL debug enabled.
- [ ] Confirm shutdown live counts are zero and no SDL validation diagnostic appears.
- [ ] Commit: `feat: complete GPU clear frame lifecycle`

**Evidence:** 120-frame log, resize dimensions, restart marker, zero counts.

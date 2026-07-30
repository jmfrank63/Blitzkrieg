# P03-M05 — Own Depth and Color Render Targets

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Create texture-backed color/depth attachments and switch target sets safely.

**Dependencies:** P03-M04, P02-M04.

**Allowed files:** `Sources/src/GFXGPU/targets.zig`, `Sources/src/GFXGPU/frame.zig`, `Sources/src/GFXGPU/renderer.zig`, `Sources/src/GFXGPU/abi.zig`, `Sources/src/GFXGPU/gfxgpu_c.h`, `Sources/src/GFXGPU/root.zig`.

- [ ] Test format usage support, matching extents/sample counts, color/depth creation rollback, target switch ending an active pass, swapchain restoration, and destruction while referenced.
- [ ] Append ABI create/destroy/bind target and bind default-target operations.
- [ ] Implement one color attachment plus optional depth/stencil for the parity milestone; reject unsupported MRT requests explicitly.
- [ ] Track load/store intent and validity per attachment for the next pass.
- [ ] Mark target textures unavailable for sampling while bound for writing.
- [ ] Extend smoke to clear an offscreen target then restore and present the swapchain.
- [ ] Commit: `feat: add GPU render targets`

**Evidence:** active-pass switch test and offscreen smoke log.

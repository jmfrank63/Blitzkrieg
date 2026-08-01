# P03-M06 — Add Screenshot Readback and Resource Stress

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Read color textures into caller-owned RGBA8 memory and prove resource lifetime under repetition.

**Dependencies:** P03-M05.

**Allowed files:** `Sources/src/GFXGPU/readback.zig`, `Sources/src/GFXGPU/lifetime.zig`, `Sources/src/GFXGPU/renderer.zig`, `Sources/src/GFXGPU/abi.zig`, `Sources/src/GFXGPU/gfxgpu_c.h`, `Sources/src/GFXGPU/root.zig`, `tools/zig/gfxgpu_smoke.cpp`.

- [ ] Add row-pitch normalization tests for RGBA8 and BGRA8, channel conversion, too-small destination, and completion-before-map.
- [ ] Append ABI readback operation taking caller destination pointer, capacity, and returned row pitch/byte count.
- [ ] Copy texture to a download transfer buffer, submit, wait only for that readback fence, map, normalize into caller memory, and unmap.
- [ ] Make smoke verify exact pixels from a known clear color.
- [ ] Add a 1,000-iteration loop creating/uploading/destroying small buffers, textures, samplers, and targets while frames advance.
- [ ] Require zero live counts, bounded cache/transfer high-water marks, and no validation diagnostics.
- [ ] Commit: `test: prove GPU resource readback and lifetime`

**Evidence:** exact pixel bytes, peak counts, final zero counts.

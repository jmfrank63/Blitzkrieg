# P03-M03 — Implement Texture Creation and Upload

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Own sampled textures with explicit format capability checks, mip layouts, and transfer uploads.

**Dependencies:** P03-M02, P01-M02.

**Allowed files:** `Sources/src/GFXGPU/textures.zig`, `Sources/src/GFXGPU/renderer.zig`, `Sources/src/GFXGPU/abi.zig`, `Sources/src/GFXGPU/gfxgpu_c.h`, `Sources/src/GFXGPU/root.zig`.

- [ ] Inventory texture formats loaded by `Sources/src/Image` and legacy `Texture.cpp`; record the concrete list in tests.
- [ ] Test dimensions/mip count, row pitch, compressed block alignment, overflow, unsupported capabilities, partial creation rollback, upload bounds, and stale handles.
- [ ] Query SDL_GPU format support before creation; return unsupported rather than substituting.
- [ ] Upload each mip through an upload transfer buffer and copy pass, preserving compressed bytes exactly.
- [ ] Store dimensions, mip count, normalized format, usage, native pointer, and last-use serial.
- [ ] Extend resource smoke with a 2×2 RGBA8 texture and one available DXT sample.
- [ ] Commit: `feat: own GPU texture resources`

**Evidence:** asset-format inventory, DXT capability result, and upload tests.

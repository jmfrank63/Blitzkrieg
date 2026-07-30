# P05-M04 — Draw Indexed and Temporary Geometry

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Complete indexed, dynamic, and temporary draw paths and a deterministic reference scene.

**Dependencies:** P05-M03.

**Allowed files:** `Sources/src/GFXGPU/draw.zig`, `Sources/src/GFXGPU/renderer.zig`, `Sources/src/GFXGPU/abi.zig`, `Sources/src/GFXGPU/gfxgpu_c.h`, `tools/zig/gfxgpu_smoke.zig`.

- [x] Append indexed draw and immediate temporary geometry ABI operations.
- [x] Test 16/32-bit index byte bounds, base vertex, first index, instance count fixed to one, temporary ring exhaustion/growth, and dynamic update-before-draw.
- [x] Encode indexed draws with explicit index format and offsets; mark every referenced resource with submission serial.
- [ ] Build smoke scene: colored triangle, textured quad using six indices, overlapping depth-tested triangles, viewport quadrant, dynamic color update, and offscreen texture sampled into swapchain.
- [ ] Read back fixed probe pixels and compare with tolerances documented in the smoke source.
- [ ] Run smoke three times in SDL debug mode and require identical hashes.
- [ ] Commit: `test: complete GPU draw reference scene`

**Evidence:** probe coordinates/expected colors, three hashes, zero validation diagnostics.

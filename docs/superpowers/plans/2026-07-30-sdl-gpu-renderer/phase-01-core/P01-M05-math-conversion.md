# P01-M05 — Define Coordinate, Viewport, and Color Conversion

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Centralize deterministic conversion from engine values to shader/SDL_GPU values.

**Dependencies:** P01-M04.

**Allowed files:** `Sources/src/GFXGPU/math_convert.zig`, `Sources/src/GFXGPU/root.zig`.

- [ ] Add golden tests for identity, translation, perspective, orthographic UI projection, matrix multiplication order, packed ARGB color, viewport, and depth range.
- [ ] Represent matrices as `[16]f32`; implement named row-major load and shader-upload conversion functions.
- [ ] Define one `ClipPolicy` selected from SDL_GPU driver metadata; policy may change Y direction and projection correction but exposes no native API type.
- [ ] Convert ARGB8 to normalized linear-order RGBA values without implicit sRGB transfer.
- [ ] Reject NaN/Inf viewport dimensions, negative sizes, and min depth greater than max depth.
- [ ] Run all pure tests three times.
- [ ] Commit: `feat: centralize renderer value conversion`

**Evidence:** exact golden matrices and three green runs.

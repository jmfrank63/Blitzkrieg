# P07-M05 — Run the Native SDL_GPU Driver Matrix

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Prove the one renderer creates, draws, presents, resizes, and shuts down with each required native driver/format.

**Dependencies:** P07-M04.

**Allowed files:** `tools/zig/gfxgpu_smoke.zig`, `build.zig`, `docs/superpowers/evidence/platform-port/target-matrix.md`, `docs/superpowers/evidence/platform-port/windows-regression.md`.

- [ ] Extend smoke arguments/build option to request a driver without environment mutation and verify the reported selected driver exactly matches.
- [ ] Run deterministic untextured, textured, alpha, depth, render-target, resize, minimize/restore, screenshot-readback, and shutdown checks.
- [ ] Execute Windows/direct3d12/DXIL, Linux/vulkan/SPIR-V, and macOS/metal/MSL on native runners.
- [ ] Require identical reference-scene dimensions/hash policy where platform rasterization is deterministic and record explained tolerance-only differences separately.
- [ ] Require all renderer live counts zero and no SDL validation/device errors.
- [ ] Commit: `test: validate native SDL GPU driver matrix`

**Evidence:** three startup/shutdown lines, reference facts, and native host details.

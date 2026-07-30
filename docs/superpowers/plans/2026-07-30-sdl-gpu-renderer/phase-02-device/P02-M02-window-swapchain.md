# P02-M02 — Claim the SDL Window and Configure the Swapchain

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Claim a provided `SDL_Window*`, choose swapchain composition/present mode, and expose its format/extent.

**Dependencies:** P02-M01.

**Allowed files:** `Sources/src/GFXGPU/surface.zig`, `Sources/src/GFXGPU/renderer.zig`, `Sources/src/GFXGPU/root.zig`, `tools/zig/gfxgpu_smoke.cpp`.

- [ ] Add fake-API tests for null window, claim failure, unsupported present mode fallback, swapchain format capture, release-before-device order, and double-release prevention.
- [ ] Treat `GfxGpuCreateInfo.sdl_window` as borrowed; Zig claims but never destroys the SDL window.
- [ ] Prefer MAILBOX when supported and otherwise VSYNC; select SDR composition for this milestone.
- [ ] Query the claimed swapchain texture format and drawable extent after claim.
- [ ] Update smoke to pass its hidden SDL window through `GfxGpuCreateInfo`.
- [ ] Run ABI and smoke tests.
- [ ] Commit: `feat: claim SDL GPU swapchain window`

**Evidence:** selected present mode, format, and extent.

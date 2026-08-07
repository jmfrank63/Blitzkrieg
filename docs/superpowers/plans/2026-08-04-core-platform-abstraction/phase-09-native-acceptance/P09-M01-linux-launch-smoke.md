# P09-M01 — Linux Native Launch Smoke

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Launch the staged Linux game through WSLg/native Linux and prove menu-level platform services.

**Dependencies:** P08-M06.

**Allowed files:** `tools/zig/linux_game_smoke.zig`, `tools/zig/verify_runtime.zig`, `build.zig`, `docs/superpowers/evidence/platform-abstraction/linux-smoke.md`.

- [x] Launch only from the staged Linux install with no source-tree library paths.
- [x] Run the native WSL platform-foundation and GFXGPU gates; lifecycle/client checks, Vulkan/SPIR-V, renderer probes, and zero-live-resource checks pass.
- [ ] Verify PlatformRuntime, SDL window, Vulkan renderer/SPIR-V, Input, SFX, Net, Main, UI, and menu initialization.
- [ ] Inject keyboard/mouse/text, resize, focus loss/restore, minimize/restore, and clean quit.
- [ ] Capture diagnostics and live-handle counts; reject validation errors and software rendering unless explicitly selected.
- [x] Repeat three launches; all three completed with exit status 0.
- [x] Commit: docs/evidence checkpoint for the staged Linux launch smoke.

**Evidence:** launch logs, GPU/backend identity, screenshots, and zero-live counts.

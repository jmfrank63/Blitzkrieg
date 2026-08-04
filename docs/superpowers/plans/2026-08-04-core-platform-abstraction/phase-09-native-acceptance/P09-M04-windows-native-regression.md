# P09-M04 — Windows Native Regression and Endurance

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Reconfirm accepted Windows gameplay after the platform cutover.

**Dependencies:** P08-M06.

**Allowed files:** `build.zig`, `tools/zig/windows_game_acceptance.zig`, `tools/zig/verify_gfxgpu_endurance.zig`, `docs/superpowers/evidence/platform-abstraction/windows-regression.md`.

- [ ] Launch the staged Windows package with PlatformRuntime and SDL_GPU Direct3D 12/DXIL.
- [ ] Repeat accepted menu, mission, input, audio, save/load, resize/fullscreen/focus, and shutdown scenarios.
- [ ] Compare deterministic screenshots against accepted Windows references and retain exact comparison metrics.
- [ ] Run lifecycle/endurance instrumentation and verify zero platform/renderer/audio/network leaks.
- [ ] Record human regression approval independently from automated results.
- [ ] Commit: `test: accept Windows platform-runtime regression`

**Evidence:** comparison metrics, lifecycle samples, logs, and human approval.

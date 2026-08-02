# P03-M05 — Complete Display Control and Bootstrap Smoke

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Implement resize, drawable size, fullscreen, minimize/restore, and a native game-window bootstrap gate.

**Dependencies:** P03-M03, P03-M04.

**Allowed files:** `Sources/src/Platform/SDLApplication.h`, `Sources/src/Platform/SDLApplication.cpp`, `Sources/src/GFXGPU/GraphicsEngineGpu.cpp`, `Sources/src/Game/GameMain.cpp`, `tools/zig/platform_window_test.cpp`, `tools/zig/game_bootstrap_smoke.cpp`, `build.zig`, `docs/superpowers/evidence/platform-port/target-matrix.md`.

- [ ] Test logical versus pixel size, zero-size/minimized suppression, resize coalescing, windowed/fullscreen round-trip, display-mode rejection, and restore redraw request.
- [ ] Implement SDL display/mode queries and window operations; notify `IGFX::SetMode`/resize only from stable non-zero dimensions.
- [ ] Add `test-game-bootstrap`, which initializes paths/modules/SDL/window/GFX, clears and presents three frames, processes a synthetic quit, and exits without loading gameplay data.
- [ ] Run it natively on Windows with `direct3d12` and Linux with `vulkan`; compile on macOS.
- [ ] Re-run Windows renderer acceptance automation and record no ownership or resize regression.
- [ ] Commit: `test: prove portable SDL game bootstrap`

**Evidence:** native driver/startup/shutdown lines and window state transitions.

# P02-M01 — SDL Lifetime and Application Window

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Move SDL init/quit and the application window into PlatformRuntime with main-thread enforcement.

**Dependencies:** P01-M06.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `Sources/src/PlatformABI/PlatformState.h`, `Sources/src/Platform/SDL/Application.cpp`, `Sources/src/Platform/SDL/Window.cpp`, `Sources/src/Platform/SDLApplication.h`, `Sources/src/Platform/SDLApplication.cpp`, `tools/zig/platform_window_test.cpp`, `build.zig`.

- [ ] Test create/show/hide/title/size/destroy, double create, wrong-thread call, and two complete restart cycles.
- [ ] Store SDL pointers only in private runtime state; expose window identity as an opaque ABI handle.
- [ ] Preserve the renderer's borrowed-window operation through a private in-process bridge.
- [ ] Verify PlatformRuntime calls `SDL_Quit` only after window and renderer release.
- [ ] Run the native window test with software and GPU-capable environments.
- [ ] Commit: `platform: own SDL application window`

**Evidence:** lifecycle trace and zero SDL/window live counts.

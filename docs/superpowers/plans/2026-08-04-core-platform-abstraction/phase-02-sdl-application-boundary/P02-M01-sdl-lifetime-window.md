# P02-M01 — SDL Lifetime and Application Window

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Move SDL init/quit and the application window into PlatformRuntime with main-thread enforcement.

**Dependencies:** P01-M06.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `Sources/src/PlatformABI/PlatformState.h`, `Sources/src/Platform/SDL/Application.cpp`, `Sources/src/Platform/SDL/Window.cpp`, `Sources/src/Platform/SDLApplication.h`, `Sources/src/Platform/SDLApplication.cpp`, `tools/zig/platform_window_test.cpp`, `build.zig`.

- [x] Run the existing Windows lifecycle contract covering initialization failure injection, create/show/hide/resize/fullscreen, clipboard/cursor, and shutdown.
- [ ] Store SDL pointers only in private runtime state; expose window identity as an opaque ABI handle.
- [x] Preserve the renderer's borrowed-window operation through the existing private `BorrowWindow` bridge.
- [x] Verify the application facade releases its window before `SDL_Quit` on shutdown.
- [x] Run the native window test on Windows; GPU-capable runtime acceptance remains open.
- [x] Commit checkpoint: `platform: own SDL application window`.

**Evidence:** the existing Windows `test-platform-window` contract is present and remains the next native SDL gate; ABI ownership conversion is still open.

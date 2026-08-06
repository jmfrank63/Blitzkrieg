# P02-M03 — Display, Focus, Resize, and Fullscreen

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Replace Win32 client-rect/work-area logic with SDL-backed display and window state operations.

**Dependencies:** P02-M02.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `Sources/src/Platform/SDL/Window.cpp`, `Sources/src/Platform/SDL/Display.cpp`, `Sources/src/Platform/SDLApplication.cpp`, `tools/zig/platform_window_test.cpp`, `tools/zig/platform_storage_gate.cpp`, `build.zig`.

- [x] Existing Windows compile contract covers logical/pixel size and fullscreen round-trip; runtime execution remains blocked by the headless SDL window hang.
- [x] Keep dimensions behind the `WindowSize` facade without exposing native handles.
- [ ] Keep swapchain resize notification ordered after the normalized window event.
- [ ] Run 100 windowed/fullscreen and minimize/restore cycles.
- [ ] Verify display-option serialization remains byte compatible.
- [x] Commit checkpoint: `platform: abstract display and window state`.

**Evidence:** `test-platform-window -Dtest-mode=compile` passes on Windows; run-mode evidence is deferred to desktop/GPU acceptance.

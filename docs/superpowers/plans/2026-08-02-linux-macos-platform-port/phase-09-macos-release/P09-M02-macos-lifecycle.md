# P09-M02 — Validate macOS Application and High-DPI Lifecycle

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Prove the shared SDL application handles macOS focus, quit, Retina sizing, fullscreen, and bundle paths.

**Dependencies:** P09-M01.

**Allowed files:** `Sources/src/Platform/SDLApplication.cpp`, `Sources/src/Platform/Paths.cpp`, `Sources/src/Game/GameMain.cpp`, `tools/zig/platform_window_test.cpp`, `build.zig`.

- [ ] Run native tests for logical/pixel dimensions at Retina scale, resize, move between displays, minimize/restore, Cmd+Q, close button, focus loss, windowed/fullscreen, text input/IME area, and bundle resource/user roots.
- [ ] Use SDL behavior unchanged where it passes. Add no Objective-C++ file unless a failing test proves an SDL gap and the coordinator approves a boundary extension.
- [ ] Ensure app events are pumped on the main thread and quit enters common game teardown before SDL/window destruction.
- [ ] Run platform window/input tests and Metal renderer smoke natively.
- [ ] Commit: `platform: validate macOS SDL lifecycle`

**Evidence:** high-DPI dimension table and ordered quit/shutdown trace.

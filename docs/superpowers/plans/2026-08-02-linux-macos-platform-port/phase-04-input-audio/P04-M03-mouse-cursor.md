# P04-M03 — Implement Mouse, Wheel, and Cursor Behavior

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Preserve pointer coordinates, button edges/double-clicks, wheel, capture, visibility, and relative mode through SDL.

**Dependencies:** P04-M01.

**Allowed files:** `Sources/src/Input/InputAPI.h`, `Sources/src/Input/InputAPI.cpp`, `Sources/src/Game/WinFrame.cpp`, `Sources/src/Scene/Cursor.cpp`, `tools/zig/platform_input_test.cpp`, `build.zig`.

- [ ] Test logical/pixel coordinate conversion, motion, three buttons, double-click thresholds, vertical/horizontal wheel, focus loss, capture toggle, relative mode, cursor show/hide, and resize scaling.
- [ ] Translate SDL mouse events once in `SDLApplication`; remove duplicate Win32 mouse emulation from `WinFrame.cpp`.
- [ ] Route capture/relative/visibility requests through SDL window/cursor functions behind the platform application seam.
- [ ] Preserve `INPUT_CONTROL_MOUSE_*` IDs and packed coordinate semantics consumed by UI/scene code.
- [ ] Run input tests at 1x and simulated 2x pixel density.
- [ ] Commit: `input: port mouse and cursor behavior to SDL`

**Evidence:** event-to-message table at both scale factors.

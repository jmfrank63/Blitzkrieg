# P04-M04 — Add Controller, Clipboard, and Input Lifecycle

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Complete SDL input device lifecycle and clipboard behavior.

**Dependencies:** P04-M02, P04-M03.

**Allowed files:** `Sources/src/Input/InputAPI.h`, `Sources/src/Input/InputAPI.cpp`, `Sources/src/Input/InputObjectFactory.cpp`, `Sources/src/Platform/SDLApplication.h`, `Sources/src/Platform/SDLApplication.cpp`, `tools/zig/platform_input_test.cpp`, `build.zig`.

- [ ] Test controller add/remove, stable slot assignment, button/axis deadzone/sign, reconnect, focus loss, clipboard UTF-8 get/set, and shutdown with queued events.
- [ ] Open/close SDL gamepads in the application owner and emit fixed platform events; Input stores no SDL pointer.
- [ ] Expose clipboard operations through the application callback and convert at the existing text boundary.
- [ ] Add `test-platform-input` as a native self-terminating window/input executable and compile-only target elsewhere.
- [ ] Run three native repetitions and Windows game startup smoke.
- [ ] Commit: `input: complete SDL device lifecycle`

**Evidence:** controller slot/reconnect trace and clipboard round-trip.

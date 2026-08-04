# P02-M02 — Normalized Event Queue

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Append fixed-layout keyboard, text, mouse, controller, window, and quit event records to the ABI.

**Dependencies:** P02-M01.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `Sources/src/Platform/Event.h`, `Sources/src/Platform/SDL/Events.cpp`, `Sources/src/Platform/SDLApplication.cpp`, `tools/zig/platform_event_test.cpp`, `build.zig`.

- [ ] Add injected SDL fixtures for key, UTF-8 text, motion, buttons, wheel, focus, resize, controller, and quit.
- [ ] Verify the test fails until event kind, timestamp, modifiers, device ID, coordinates, and payload sizes are stable.
- [ ] Translate SDL events once and queue bounded ABI records in arrival order.
- [ ] Define overflow behavior and emit one diagnostic per overflow episode.
- [ ] Assert gameplay consumers do not include SDL event headers.
- [ ] Commit: `platform: expose normalized application events`

**Evidence:** byte-for-byte event fixture records.

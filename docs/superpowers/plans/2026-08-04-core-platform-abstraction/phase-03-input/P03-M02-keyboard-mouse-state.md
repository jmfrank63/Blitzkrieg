# P03-M02 — Implement Event-Fed Keyboard and Mouse State

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Drive key, button, axis, wheel, and pointer state from normalized platform events.

**Dependencies:** P03-M01.

**Allowed files:** `Sources/src/Input/InputAPI.cpp`, `Sources/src/Input/InputCodes.cpp`, `Sources/src/Input/Visitors.cpp`, `tools/zig/platform_input_test.cpp`, `tools/zig/input_state_test.cpp`, `build.zig`.

- [ ] Add fixtures for press/release, simultaneous modifiers, motion, wheel direction, buttons, focus loss, and same-frame ordering.
- [ ] Run the fixture against the old Windows oracle and record stable legacy control IDs and values.
- [ ] Replace device polling and buffered DirectInput records with platform event consumption.
- [ ] Synthesize releases on focus loss and preserve mouse-coordinate translation semantics.
- [ ] Run the state fixture on Windows and Linux.
- [ ] Commit: `input: consume platform keyboard and mouse events`

**Evidence:** identical legacy event/value fixture on Windows and Linux.

# P03-M02 — Implement Event-Fed Keyboard and Mouse State

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Drive key, button, axis, wheel, and pointer state from normalized platform events.

**Dependencies:** P03-M01.

**Allowed files:** `Sources/src/Input/InputAPI.cpp`, `Sources/src/Input/InputCodes.cpp`, `Sources/src/Input/Visitors.cpp`, `tools/zig/platform_input_test.cpp`, `tools/zig/input_state_test.cpp`, `build.zig`.

- [x] Add a focused event-fed fixture for press/release, simultaneous modifiers, motion, wheel direction, buttons, focus loss, and same-frame ordering.
- [ ] Run the fixture against the old Windows oracle and record stable legacy control IDs and values.
- [ ] Replace device polling and buffered DirectInput records with platform event consumption.
- [ ] Synthesize releases on focus loss and preserve mouse-coordinate translation semantics.
- [x] Run the state fixture on Windows. Linux execution remains deferred to the cross-platform validation environment.
- [x] Commit checkpoint: `097a5fe24` added the event-fed fixture and `bd66b84f0` closed its Windows build graph.

**Evidence:** `test-input-state` compiles and runs on the Windows target. The fixture records the legacy control IDs and event ordering at the normalized `PlatformEvent` boundary. `CInputAPI` does not currently expose inspectable state independently of its private DirectInput-backed device graph, so this packet deliberately adds no new public state API; Linux execution and the old-oracle comparison remain open.

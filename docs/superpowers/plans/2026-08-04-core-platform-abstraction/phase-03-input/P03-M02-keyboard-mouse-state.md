# P03-M02 — Implement Event-Fed Keyboard and Mouse State

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Drive key, button, axis, wheel, and pointer state from normalized platform events.

**Dependencies:** P03-M01.

**Allowed files:** `Sources/src/Input/InputAPI.cpp`, `Sources/src/Input/InputCodes.cpp`, `Sources/src/Input/Visitors.cpp`, `tools/zig/platform_input_test.cpp`, `tools/zig/input_state_test.cpp`, `build.zig`.

- [x] Add a focused event-fed fixture for press/release, simultaneous modifiers, motion, wheel direction, buttons, focus loss, and same-frame ordering.
- [ ] Run the fixture against the old Windows oracle and record stable legacy control IDs and values.
- [x] Replace device polling and buffered DirectInput records with platform event consumption in the production `BK_INPUT_EVENT_ONLY` graph.
- [x] Synthesize releases on focus loss and preserve mouse-coordinate translation semantics.
- [x] Run the state fixture on Windows. Linux execution remains deferred to the cross-platform validation environment.
- [x] Commit checkpoint: `097a5fe24` added the event-fed fixture and `bd66b84f0` closed its Windows build graph.

**Evidence:** `zig build input -Dtarget=x86_64-windows-msvc`, `test-input-state`, and `test-platform-input` pass. The event-only `CInputAPI` owns virtual keyboard/mouse controls, consumes `SInputEvent` records in source order, uses platform monotonic time, and synthesizes releases on focus loss. The standalone fixture records stable legacy IDs and event ordering; old-oracle comparison and Linux execution remain open.

# P03-M04 — Controller Device Mapping

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Map SDL controller discovery, buttons, hats, and axes to stable legacy device/control descriptions.

**Dependencies:** P03-M03.

**Allowed files:** `Sources/src/Input/InputAPI.cpp`, `Sources/src/Input/InputCodes.h`, `Sources/src/Input/InputCodes.cpp`, `tools/zig/input_controller_test.cpp`, `build.zig`.

- [ ] Add virtual-controller fixtures for connect, duplicate name, disconnect, reconnect, dead zone, signed axes, triggers, and stale events.
- [ ] Define deterministic device ordering and stable runtime IDs without serializing transient host IDs.
- [ ] Map normalized values to `AXIS_RANGE_VALUE` and preserve inversion/power rules.
- [ ] Release active controls before device removal.
- [ ] Run fixtures on all target compilers and native Windows/Linux.
- [ ] Commit: `input: map portable controller devices`

**Evidence:** device/control descriptor fixture and hotplug timeline.

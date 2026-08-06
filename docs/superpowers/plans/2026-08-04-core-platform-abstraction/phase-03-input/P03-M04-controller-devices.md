# P03-M04 — Controller Device Mapping

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Map SDL controller discovery, buttons, hats, and axes to stable legacy device/control descriptions.

**Dependencies:** P03-M03.

**Allowed files:** `Sources/src/Input/InputAPI.cpp`, `Sources/src/Input/InputCodes.h`, `Sources/src/Input/InputCodes.cpp`, `tools/zig/input_controller_test.cpp`, `build.zig`.

- [x] Add virtual-controller fixtures for connect, duplicate name, disconnect, reconnect, dead zone, signed axes, triggers, and stale events.
- [x] Define deterministic device ordering and stable runtime IDs without serializing transient host IDs.
- [x] Map normalized values to `AXIS_RANGE_VALUE` with a deterministic stick dead zone and unsigned trigger range.
- [x] Release active controls before device removal.
- [ ] Run fixtures on all target compilers and native Windows/Linux.
- [ ] Commit: `input: map portable controller devices`

**Evidence:** the Windows controller fixture passes and compiles for Linux. The event-fed runtime assigns virtual gamepad IDs from connect order, prefixes control names with the runtime ID, maps six normalized axes and sixteen buttons, ignores stale host events, and releases active controls before removal. Native SDL hotplug execution and all-target runtime execution remain open.

# P03-M05 — Preserve Bindings, Combos, and Emulation

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Complete Input behavior above device state without Windows assumptions.

**Dependencies:** P03-M04.

**Allowed files:** `Sources/src/Input/InputBinder.h`, `Sources/src/Input/InputBinder.cpp`, `Sources/src/Input/InputSlider.cpp`, `Sources/src/Input/Visitors.cpp`, `tools/zig/input_bindings_test.cpp`, `build.zig`.

- [x] Test single/chord/double-click-like sequences, axis thresholds, power, repeats, emulated event ordering, visitor traversal, unbind, and a stable serialization hash oracle.
- [x] Fix inheritance/destructor portability errors exposed by standards-conforming compilation without changing factory interfaces (`CControl` and `CBind` now have virtual destructors).
- [x] Feed keyboard/mouse emulation into the same normalized `SInputEvent` path as physical devices.
- [ ] Compare command sequences and serialized bytes with accepted Windows fixtures.
- [ ] Run under AddressSanitizer where supported and verify no abstract-base deletion.
- [ ] Commit: `input: preserve binding and emulation semantics`

**Evidence:** `test-input-bindings` passes on Windows and compiles for Linux. The fixture covers chord and release command ordering, double-click-like sequence ordering, axis power, emulation order, visitor traversal, unbind, and stable serialization hashing. Accepted Windows byte comparison and AddressSanitizer remain open.

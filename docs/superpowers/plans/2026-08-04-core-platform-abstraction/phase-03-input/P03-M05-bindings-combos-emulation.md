# P03-M05 — Preserve Bindings, Combos, and Emulation

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Complete Input behavior above device state without Windows assumptions.

**Dependencies:** P03-M04.

**Allowed files:** `Sources/src/Input/InputBinder.h`, `Sources/src/Input/InputBinder.cpp`, `Sources/src/Input/InputSlider.cpp`, `Sources/src/Input/Visitors.cpp`, `tools/zig/input_bindings_test.cpp`, `build.zig`.

- [ ] Test single/chord/double-click bindings, axis thresholds, power, repeats, emulated events, visitor traversal, unbind, and serialization round-trip.
- [ ] Fix inheritance/destructor portability errors exposed by standards-conforming Linux compilation without changing factory interfaces.
- [ ] Feed emulation into the same normalized event path as physical devices.
- [ ] Compare command sequences and serialized bytes with accepted Windows fixtures.
- [ ] Run under AddressSanitizer where supported and verify no abstract-base deletion.
- [ ] Commit: `input: preserve binding and emulation semantics`

**Evidence:** command sequence, serialization hash, and sanitizer summary.

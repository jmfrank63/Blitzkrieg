# P02-M01 — Implement the Used Legacy Variant Subset

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Remove the Linux/macOS dependency on `<comutil.h>`, `VARIANT`, and BSTR allocation while preserving option/manipulator semantics.

**Dependencies:** P01-M05.

**Allowed files:** `Sources/src/Platform/LegacyVariant.h`, `Sources/src/Platform/LegacyVariant.cpp`, `Sources/src/Misc/Basic.h`, `Sources/src/Misc/VarSystemInternal.h`, `tools/zig/legacy_variant_test.cpp`, `build.zig`.

- [ ] Inventory used tags and lock the subset: empty, null, bool, UI1, I2, I4, UI8, R4, R8, date, string, error, dispatch/unknown comparison only.
- [ ] Test construction, copy/move, assignment, destruction, numeric/string conversion, boolean values, tag preservation, deep string ownership, and serialization round-trip for each used scalar.
- [ ] On Windows alias/adapt the existing COM-backed behavior; on non-Windows provide a C++ value type with identical source-level operators and fixed numeric tag constants.
- [ ] Replace unconditional COM includes in `Basic.h`; keep unsupported object conversion an explicit failure.
- [ ] Run Windows and Linux variant tests and `streamio` unit tests.
- [ ] Commit: `platform: provide portable legacy variants`

**Evidence:** tag/value round-trip table and leak-free copy/destruction test.

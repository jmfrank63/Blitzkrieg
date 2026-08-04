# P03-M01 — Remove DirectInput from Input Types

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Replace DirectInput objects and records in Input headers with backend-neutral state owned by Input.

**Dependencies:** P02-M05.

**Allowed files:** `Sources/src/Input/Specific.h`, `Sources/src/Input/InputAPI.h`, `Sources/src/Input/InputCodes.h`, `tools/zig/input_api_headers_test.cpp`, `tools/zig/runtime_platform_audit.zig`, `build.zig`.

- [ ] Add a Linux C++ header test that includes every public/private Input header and rejects DirectInput identifiers.
- [ ] Introduce `SInputDevice`, `SInputEvent`, and fixed-width control data using platform device IDs and legacy control IDs.
- [ ] Preserve class inheritance, factory interfaces, serialized descriptors, and object sizes that cross existing module interfaces.
- [ ] Remove `dinput.h`, `IDirectInput*`, `DIDEVICE*`, COM pointers, and DirectInput GUIDs from portable headers.
- [ ] Compile the header test for all triples.
- [ ] Commit: `input: remove DirectInput from module types`

**Evidence:** zero DirectInput audit hits in headers and ABI-size report.

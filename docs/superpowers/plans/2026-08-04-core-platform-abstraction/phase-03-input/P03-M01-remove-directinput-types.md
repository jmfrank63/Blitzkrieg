# P03-M01 — Remove DirectInput from Input Types

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Replace DirectInput objects and records in Input headers with backend-neutral state owned by Input.

**Dependencies:** P02-M05.

**Allowed files:** `Sources/src/Input/Specific.h`, `Sources/src/Input/InputAPI.h`, `Sources/src/Input/InputCodes.h`, `tools/zig/input_api_headers_test.cpp`, `tools/zig/runtime_platform_audit.zig`, `build.zig`.

- [ ] Add a Linux C++ header test that includes every public/private Input header and rejects DirectInput identifiers.
- [ ] Introduce `SInputDevice`, `SInputEvent`, and fixed-width control data using platform device IDs and legacy control IDs.
- [ ] Preserve class inheritance, factory interfaces, serialized descriptors, and object sizes that cross existing module interfaces.
- [ ] Remove `dinput.h`, `IDirectInput*`, `DIDEVICE*`, COM pointers, and DirectInput GUIDs from portable headers.
- [x] Compile the existing Windows input code/mapping gates; all-triple header decontamination remains open.
- [x] Commit checkpoint: `input: remove DirectInput from module types`.

**Evidence:** Windows `test-platform-input -Dtest-mode=compile` and native `test-input-codes -Dtest-mode=run` pass; DirectInput header/type removal is not yet claimed.

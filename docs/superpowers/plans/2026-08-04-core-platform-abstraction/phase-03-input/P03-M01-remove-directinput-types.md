# P03-M01 — Remove DirectInput from Input Types

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Replace DirectInput objects and records in Input headers with backend-neutral state owned by Input.

**Dependencies:** P02-M05.

**Allowed files:** `Sources/src/Input/Specific.h`, `Sources/src/Input/InputTypes.h`, `Sources/src/Input/InputAPI.h`, `Sources/src/Input/InputCodes.h`, `tools/zig/input_headers_test.cpp`, `tools/zig/runtime_platform_audit.zig`, `build.zig`.

- [x] Add a Linux C++ header test that includes every public/private Input header and rejects DirectInput identifiers.
- [x] Introduce the production event-fed virtual keyboard/mouse state using normalized platform events while preserving legacy device/control IDs.
- [ ] Preserve class inheritance, factory interfaces, serialized descriptors, and object sizes that cross existing module interfaces.
- [x] Remove `dinput.h`, `IDirectInput*`, `DIDEVICE*`, COM pointers, and DirectInput GUIDs from the event-only portable headers; the legacy oracle declarations remain isolated behind the legacy compile branch.
- [x] Compile the existing Windows input code/mapping gates; all-triple header decontamination remains open.
- [x] Commit checkpoint: `input: remove DirectInput from module types`.

**Evidence:** `zig build test-input-headers -Dtarget=x86_64-windows-msvc -Dtest-mode=compile` and the Linux compile gate pass; the Windows audit executable also passes. `zig build input -Dtarget=x86_64-windows-msvc`, `test-input-state`, and `test-platform-input` pass. `CInputAPI::Init` creates virtual keyboard/mouse devices with legacy IDs and the event-only graph has no visible DirectInput declarations. The legacy source branch and `dinput8`/`dxguid` links remain for the next controller/oracle cleanup packet.

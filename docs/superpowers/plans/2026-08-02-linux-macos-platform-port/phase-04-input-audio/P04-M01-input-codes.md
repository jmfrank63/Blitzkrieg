# P04-M01 — Preserve Legacy Input Codes Without DirectInput

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Replace DIK/GUID/device enumeration dependencies while preserving existing keybind names and numeric IDs.

**Dependencies:** P03-M05.

**Allowed files:** `Sources/src/Input/InputCodes.h`, `Sources/src/Input/InputCodes.cpp`, `Sources/src/Input/InputTypes.h`, `Sources/src/Input/Specific.h`, `Sources/src/Input/InputAPI.h`, `tools/zig/input_codes_test.cpp`, `build.zig`.

- [ ] Snapshot every existing keyboard/mouse/controller name-to-ID pair used by configs and test uniqueness plus reverse lookup.
- [ ] Define engine-owned device/control descriptors and constants; map SDL scancodes to the existing DIK numeric values rather than renumbering configs.
- [ ] Remove `<dinput.h>`, GUID, `DIDEVICE*`, and COM pointer types from Input public/private headers.
- [ ] Keep unknown keys explicit and preserve keyboard, mouse, first controller device names expected by bind files.
- [ ] Run mapping tests against tracked `Data/Configs` keybind inputs.
- [ ] Commit: `input: preserve legacy codes without DirectInput`

**Evidence:** mapping snapshot hash and zero duplicate IDs.

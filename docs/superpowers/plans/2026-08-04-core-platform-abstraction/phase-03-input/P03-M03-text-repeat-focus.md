# P03-M03 — Text, Repeat, and Focus Semantics

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Preserve legacy text entry and repeat timing without `SystemParametersInfo` or DirectInput polling.

**Dependencies:** P03-M02.

**Allowed files:** `Sources/src/Input/InputAPI.cpp`, `Sources/src/Input/InputBinder.cpp`, `Sources/src/Input/InputSlider.cpp`, `tools/zig/input_text_repeat_test.cpp`, `build.zig`.

- [ ] Test UTF-8 input, legacy text conversion, dead-key/composition boundaries, backspace, repeat delay/rate, focus loss, and disabled text mode.
- [ ] Store repeat policy in Input using platform monotonic time and deterministic defaults; accept platform-provided settings only through ABI records.
- [ ] Keep key events and text events separate to prevent duplicate characters.
- [ ] Verify saved keybind names and command messages remain unchanged.
- [ ] Run deterministic simulated-time tests and native text smoke.
- [ ] Commit: `input: port text repeat and focus behavior`

**Evidence:** exact text/repeat timeline and unchanged binding fixture.

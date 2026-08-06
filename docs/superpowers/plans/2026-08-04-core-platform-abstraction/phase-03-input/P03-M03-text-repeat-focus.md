# P03-M03 — Text, Repeat, and Focus Semantics

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Preserve legacy text entry and repeat timing without `SystemParametersInfo` or DirectInput polling.

**Dependencies:** P03-M02.

**Allowed files:** `Sources/src/Input/InputAPI.cpp`, `Sources/src/Input/InputBinder.cpp`, `Sources/src/Input/InputSlider.cpp`, `tools/zig/input_text_repeat_test.cpp`, `build.zig`.

- [ ] Test UTF-8 input, legacy text conversion, dead-key/composition boundaries, backspace, repeat delay/rate, focus loss, and disabled text mode.
- [x] Store deterministic repeat delay/rate defaults in Input and use platform monotonic time on the event-fed path; platform text events remain the source of composed/repeated characters.
- [x] Keep key events and text events separate to prevent duplicate characters.
- [ ] Verify saved keybind names and command messages remain unchanged.
- [x] Run deterministic simulated-time tests; native text smoke remains open.
- [ ] Commit: `input: port text repeat and focus behavior`

**Evidence:** `zig build test-input-text-repeat -Dtarget=x86_64-windows-msvc -Dtest-mode=run` passes, and the fixture compiles for Linux. It covers UTF-8 decoding, separate key/text streams, explicit 500 ms/30 ms repeat timing, disabled text mode, and focus-loss repeat cancellation. Dead-key/composition, backspace, native text smoke, and binding-name verification remain open.

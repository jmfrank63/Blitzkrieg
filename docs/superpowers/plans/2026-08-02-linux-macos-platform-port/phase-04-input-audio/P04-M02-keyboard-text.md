# P04-M02 — Implement Keyboard, Text, Repeat, and Focus

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Feed keyboard and Unicode text behavior from platform events without DirectInput polling.

**Dependencies:** P04-M01.

**Allowed files:** `Sources/src/Input/Input.h`, `Sources/src/Input/InputAPI.h`, `Sources/src/Input/InputAPI.cpp`, `Sources/src/Input/InputBinder.cpp`, `Sources/src/Main/Initialization.cpp`, `tools/zig/platform_input_test.cpp`, `build.zig`.

- [ ] Add deterministic event tests for key down/up, modifiers, repeats, double press timing, focus loss release, text mode transitions, UTF-8 input, backspace/enter/escape, and queue clearing.
- [ ] Replace DirectInput creation/device enumeration/acquire/poll with `NPlatform::Event` ingestion and `Clock` timestamps.
- [ ] Change `IInput::Init(HWND)` and `CInputAPI::Init(HWND)` to a window-free `Init()` contract and update `NMain::Initialize`'s input call; the SDL event owner supplies all input state.
- [ ] Start/stop SDL text input through an injected application callback when `SetTextMode` changes; do not synthesize text from keycodes.
- [ ] Preserve configured repeat suppression and legacy game-message ordering.
- [ ] Search `InputAPI.cpp` for DirectInput/WinMM symbols; expect none.
- [ ] Commit: `input: consume SDL keyboard and text events`

**Evidence:** ordered message queue fixtures and DirectInput-free search.

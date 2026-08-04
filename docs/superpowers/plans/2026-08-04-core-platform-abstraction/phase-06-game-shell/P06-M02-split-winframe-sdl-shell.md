# P06-M02 — Split WinFrame from the SDL Game Shell

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Move application window/message behavior out of `WinFrame.cpp` and retain only Windows splash/resource code there.

**Dependencies:** P06-M01.

**Allowed files:** `Sources/src/Game/WinFrame.cpp`, `Sources/src/Game/GameFrame.h`, `Sources/src/Game/GameFrame.cpp`, `Sources/src/Game/GameMain.cpp`, `tools/zig/game_frame_test.cpp`, `build.zig`.

- [ ] Capture Windows oracle traces for create/show/focus/resize/minimize/close and client coordinates.
- [ ] Implement `GameFrame` over platform window/events with no `HWND`, WNDPROC, work-area, or client-rect calls.
- [ ] Limit `WinFrame.cpp` to splash/resources and compile it only for Windows.
- [ ] Route normalized events to existing game message structures without duplicate mouse events.
- [ ] Compare lifecycle/event traces on Windows and Linux.
- [ ] Commit: `game: replace WinFrame with SDL shell`

**Evidence:** oracle-compatible event/lifecycle trace.

# P06-M02 — Split WinFrame from the SDL Game Shell

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Move application window/message behavior out of `WinFrame.cpp` and retain only Windows splash/resource code there.

**Dependencies:** P06-M01.

**Allowed files:** `Sources/src/Game/WinFrame.cpp`, `Sources/src/Game/GameFrame.h`, `Sources/src/Game/GameFrame.cpp`, `Sources/src/Game/GameMain.cpp`, `tools/zig/game_frame_test.cpp`, `build.zig`.

- [ ] Capture Windows oracle traces for create/show/focus/resize/minimize/close and client coordinates.
- [x] Implement `GameFrame` over platform window/events with no `HWND`, WNDPROC, work-area, or client-rect calls.
- [ ] Limit `WinFrame.cpp` to splash/resources and compile it only for Windows.
- [ ] Route normalized events to existing game message structures without duplicate mouse events.
- [ ] Compare lifecycle/event traces on Windows and Linux.
- [ ] Commit: `game: replace WinFrame with SDL shell`

**Evidence:** Windows `test-game-frame -Dtarget=x86_64-windows-msvc -Dtest-mode=compile` and `-Dtest-mode=run` pass for the new `GameFrame` SDL-owned lifecycle/event-queue contract. The Linux compile leg reaches Zig's SDL shared-library symlink step but is blocked by `PermissionDenied` creating `libSDL3.so.0` in the current environment. The legacy `WinFrame.cpp` still owns the Windows adapter and its full migration, oracle trace, and duplicate-event comparison remain open.

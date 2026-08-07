# P06-M03 — System Keys and Input Routing

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Replace keyboard hooks, async key polling, and screensaver calls with platform events and window policy.

**Dependencies:** P06-M02, P03-M06.

**Allowed files:** `Sources/src/Game/SysKeys.cpp`, `Sources/src/Game/SysKeys.h`, `Sources/src/Game/GameFrame.cpp`, `Sources/src/GameTT/iMissionInternal.cpp`, `tools/zig/game_system_keys_test.cpp`, `build.zig`.

- [x] Test Alt+Enter, speed plus/minus, focus loss, suppressed system combinations, quit, and key-up cleanup.
- [ ] Replace `SetWindowsHookEx`, `GetAsyncKeyState`, and `SystemParametersInfo` with normalized events and explicit game policy.
- [x] Keep OS-reserved shortcuts under SDL/OS control and preserve gameplay command IDs.
- [ ] Feed Input exactly once from the Game event loop.
- [ ] Run the fixture on Windows/Linux and compile macOS.
- [x] Commit: `game: port system keys to platform events`

**Evidence:** `test-game-system-keys -Dtarget=x86_64-windows-msvc -Dtest-mode=run` passes synthetic Alt+Enter toggle, Ctrl+Escape mouse release, plus/minus pass-through, GUI shortcut suppression, focus-loss cleanup, quit pass-through, and key-up cleanup. The same fixture compiles for `x86_64-linux-gnu`; the macOS compile command is wired but the current host lacks the required SDL sysroot. The old WinFrame/SysKeys integration and full hook-token audit remain open until the shell is switched over.

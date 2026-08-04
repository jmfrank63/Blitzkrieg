# P06-M03 — System Keys and Input Routing

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Replace keyboard hooks, async key polling, and screensaver calls with platform events and window policy.

**Dependencies:** P06-M02, P03-M06.

**Allowed files:** `Sources/src/Game/SysKeys.cpp`, `Sources/src/Game/SysKeys.h`, `Sources/src/Game/GameFrame.cpp`, `Sources/src/GameTT/iMissionInternal.cpp`, `tools/zig/game_system_keys_test.cpp`, `build.zig`.

- [ ] Test Alt+Enter, speed plus/minus, focus loss, suppressed system combinations, quit, and key-up cleanup.
- [ ] Replace `SetWindowsHookEx`, `GetAsyncKeyState`, and `SystemParametersInfo` with normalized events and explicit game policy.
- [ ] Keep OS-reserved shortcuts under SDL/OS control and preserve gameplay command IDs.
- [ ] Feed Input exactly once from the Game event loop.
- [ ] Run the fixture on Windows/Linux and compile macOS.
- [ ] Commit: `game: port system keys to platform events`

**Evidence:** command/event fixture and zero hook/poll audit hits.

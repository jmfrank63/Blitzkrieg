# P03-M03 — Translate SDL Events and Retire the Win32 Pump

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Drive quit, focus, move, resize, minimize/restore, and input delivery from SDL events.

**Dependencies:** P03-M02.

**Allowed files:** `Sources/src/Platform/Event.h`, `Sources/src/Platform/SDLApplication.cpp`, `Sources/src/Game/WinFrame.h`, `Sources/src/Game/WinFrame.cpp`, `Sources/src/Game/SysKeys.h`, `Sources/src/Game/SysKeys.cpp`, `Sources/src/Game/GameMain.cpp`, `tools/zig/platform_event_test.cpp`, `build.zig`.

- [ ] Test conversion for every `EventType` in `README.md`, timestamp monotonicity, UTF-8 text truncation/NUL termination, wheel sign, mouse coordinates, repeat flag, and unknown-event ignore.
- [ ] Drain `SDL_PollEvent` once per game-loop iteration and route window events to application state and input events to the Input module seam introduced for Phase 04.
- [ ] Replace Win32 `PeekMessage`/WndProc ownership with a compatibility `NWinFrame` facade backed by `SDLApplication`; leave only splash/resource behavior in the Windows implementation.
- [ ] Replace syskey handling with translated modifier/key records; keep Alt+Enter/fullscreen and quit semantics.
- [ ] Run synthetic event tests and a 500-cycle show/resize/minimize/restore/quit queue stress.
- [ ] Commit: `platform: drive the game loop from SDL events`

**Evidence:** event conversion matrix and stress completion.

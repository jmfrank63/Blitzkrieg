# P06-M05 — Main Loop, Focus, Quit, and Restart

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Complete the portable application loop and deterministic shutdown/restart sequence.

**Dependencies:** P06-M04.

**Allowed files:** `Sources/src/Game/GameMain.cpp`, `Sources/src/Game/GameFrame.cpp`, `Sources/src/Main/iMainInternal.cpp`, `Sources/src/Main/MainLoopCommands.cpp`, `tools/zig/game_loop_test.cpp`, `build.zig`.

- [ ] Test idle, focused/unfocused pacing, minimized rendering suspension, resize, quit request, emergency save request, and full restart.
- [ ] Replace direct sleep/tick calls with platform clock services and simulated-time hooks in tests.
- [ ] Ensure renderer ends/drains before window destruction and modules stop before PlatformRuntime destruction.
- [ ] Run three complete bootstrap/main-loop/shutdown cycles.
- [ ] Verify identical lifecycle ordering on Windows/Linux.
- [x] Commit: `game: port main loop and restart lifecycle`

**Evidence:** `test-game-loop -Dtarget=x86_64-windows-msvc -Dtest-mode=run` passes simulated focus/unfocus pacing, minimize/restore suspension, resize preservation, quit handling, reset/restart, and three identical `RsRs` lifecycle traces. The deterministic policy compiles for Linux once the SDL shared-library symlink environment permits; integration into the legacy `GameMain` loop and native desktop lifecycle comparison remain open.

# P06-M01 — Portable Entry Point and Command Line

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Make one portable `main` call the common game body and keep Windows resource startup in a thin adapter.

**Dependencies:** P02-M05.

**Allowed files:** `Sources/src/Game/main.cpp`, `Sources/src/Game/GameMain.h`, `Sources/src/Game/GameMain.cpp`, `Sources/src/Game/WindowsMain.cpp`, `tools/zig/game_command_line_test.cpp`, `build.zig`.

- [ ] Test UTF-8 arguments, spaces, empty argument, renderer option, data root, help/error exit, and Windows wide-command-line conversion.
- [ ] Move common startup into `RunGame(BkGameLaunchInfo)` with no Windows types.
- [ ] Compile `WindowsMain.cpp` and resources only for Windows; compile portable `main.cpp` for Linux/macOS.
- [ ] Create PlatformRuntime before any gameplay module and pass normalized arguments/roots.
- [ ] Run command-line tests for all target compilers and natively on Windows/Linux.
- [ ] Commit: `game: add portable entry and launch contract`

**Evidence:** exact normalized argument vectors and exit codes.

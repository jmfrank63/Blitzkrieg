# P03-M01 — Extract a Portable Game Entry Point

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Move game startup/shutdown out of WinMain and parse an argument vector without Windows command-line APIs.

**Dependencies:** P02-M06.

**Allowed files:** `Sources/src/Game/GameMain.h`, `Sources/src/Game/GameMain.cpp`, `Sources/src/Game/main.cpp`, `Sources/src/Game/StdAfx.h`, `tools/zig/game_command_line_test.cpp`, `build.zig`.

- [ ] Add parser tests for all existing flags, quoted map/save/mod/movie paths, startup smoke, reference scene dimensions, multiplayer host/password, unknown flags, and empty arguments.
- [ ] Move `SCmdParams` and `ProcessCommandLine` to an argv-based helper without changing default values.
- [ ] Extract the current WinMain body into `int GameMain(const NPlatform::Arguments&)`; make resource/log/path calls use Phase 01/02 facades.
- [ ] Add a conventional `main(int, char**)` for Linux/macOS and retain a thin Windows GUI entry adapter that calls the same function.
- [ ] Keep Windows CRT debug, SEH, and resource splash code inside `_WIN32` branches; non-Windows startup must not compile those declarations.
- [ ] Run command-line tests and Windows `game-all`.
- [ ] Commit: `platform: extract portable game entry point`

**Evidence:** parser case table and successful Windows link.

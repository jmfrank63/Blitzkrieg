# P06-M04 — Link the Complete Linux Runtime

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Compile and link all playable modules plus `Game` for `x86_64-linux-gnu`.

**Dependencies:** P06-M02, P06-M03.

**Allowed files:** `build.zig`, `Sources/src/Game/main.cpp`, `Sources/src/Main/Initialization.cpp`, `Sources/src/Main/iMainInternal.cpp`, `Sources/src/Main/MainLoopCommands.cpp`, `Sources/src/Main/LoadDLLs.cpp`, `Sources/src/GFXGPU/GraphicsEngineGpu.cpp`, `Sources/src/SFX/AudioBackendOpen.cpp`, `Sources/src/Net/NetLowest.cpp`, `docs/superpowers/evidence/platform-port/target-matrix.md`.

- [ ] Split common, Windows, Linux, and macOS flags; Linux must not receive `_WIN32`, MSVC SDK paths, `.def`, resources, Windows subsystem/entry, MSVC CRT, ODBC, COM, WinMM, User32, GDI, DirectInput, or WinSock libraries.
- [ ] Build in dependency order: zlib/libpng/Misc/Formats/Common/RandomMapGen, StreamIO/options, Image/Anim/UI/Input/SFX/Net/GFXGPU, Scene/AILogic/GameTT/Main, then `Game`.
- [ ] Link only Linux libraries required by compiled code and SDL/miniaudio; do not add libraries merely to silence unused Windows declarations.
- [ ] Run `zig build game-all -Dtarget=x86_64-linux-gnu -Doptimize=Debug -Dtest-mode=compile` from a clean cache twice.
- [ ] Record final artifact names and unresolved-symbol absence in the target matrix.
- [ ] Commit: `build: link complete Linux game runtime`

**Evidence:** full Linux artifact list and successful final link.

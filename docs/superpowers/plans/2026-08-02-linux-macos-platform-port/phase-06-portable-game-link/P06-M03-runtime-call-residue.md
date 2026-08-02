# P06-M03 — Remove Known Platform Calls from Gameplay Modules

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Route the known direct clock, sleep, file, executable-path, and process calls in full-game modules through completed platform services.

**Dependencies:** P06-M01.

**Allowed files:** `Sources/src/Common/InterfaceScreenBase.cpp`, `Sources/src/Scene/OpenVideoPlayer.cpp`, `Sources/src/Scene/Transition.cpp`, `Sources/src/Scene/VideoPlayer.cpp`, `Sources/src/GameTT/Chapter.cpp`, `Sources/src/GameTT/Common.cpp`, `Sources/src/GameTT/iMissionInternal.cpp`, `Sources/src/GameTT/InterfaceOptionsSettings.cpp`, `Sources/src/GameTT/MainMenu.cpp`, `Sources/src/GameTT/OptionEntryWrapper.cpp`, `Sources/src/RandomMapGen/MapInfo_StaticMethods_RMGeneration.cpp`, `Sources/src/RandomMapGen/Resource_Functions.cpp`, `Sources/src/Main/GameDB.cpp`, `Sources/src/UI/UIScreen.cpp`, `Sources/src/AILogic/AIUnit.cpp`, `Sources/src/AILogic/DamageToEnemyUpdater.cpp`, `tools/zig/runtime_platform_audit.zig`, `build.zig`.

- [ ] Add focused tests for transition/video/game-loop elapsed time, random seed source, small-file checksum read, main-menu executable path, and external process completion.
- [ ] Replace `timeGetTime`, `GetTickCount`, `Sleep`, `CreateFile`, `GetModuleFileName`, and process calls with Clock, FileUtils, Paths, and System services.
- [ ] Replace `_stricmp`/`_strnicmp`, `_finite`, `_itoa`, `_access`, and `MAX_PATH` uses in allowed runtime files with standard/platform helpers while preserving ASCII case-insensitive resource semantics.
- [ ] Remove `<mmsystem.h>` and native process/file declarations from allowed files.
- [ ] Add an audit limited to playable source lists that rejects known direct API tokens outside platform implementations.
- [ ] Run the audit and affected unit tests; preserve timing constants and file bytes.
- [ ] Commit: `platform: remove gameplay Win32 call residue`

**Evidence:** zero forbidden audit hits and unchanged fixture outputs.

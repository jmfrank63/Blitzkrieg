# P06-M04 — Module Loading and Startup Errors

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Route module loading, symbol lookup, module paths, diagnostics, and startup failure presentation through PlatformRuntime.

**Dependencies:** P06-M03.

**Allowed files:** `Sources/src/Game/GameMain.cpp`, `Sources/src/Main/LoadDLLs.cpp`, `Sources/src/Main/Initialization.cpp`, `Sources/src/Main/MainLoopCommands.cpp`, `tools/zig/game_module_startup_test.cpp`, `build.zig`.

- [ ] Test missing module, wrong descriptor, ABI mismatch, factory failure, partial startup rollback, reverse unload, and diagnostic text.
- [ ] Replace native loader/shell calls with PlatformClient and platform filename policy.
- [ ] Load PlatformRuntime before all gameplay modules and unload it last.
- [ ] Verify every successful module load has one unload on every failure path.
- [ ] Run injected-failure permutations natively on Windows/Linux.
- [x] Commit: `game: port module startup and rollback`

**Evidence:** module loading is no longer performed by static initialization. `LoadAllModules` now owns descriptor/factory registration and is called explicitly only after command-line help/error validation and path initialization; `NMain::Finalize` remains the unload path. The Windows `Game.exe -help` smoke exits `0` without attempting DLL loads. Missing-module rollback, PlatformRuntime-before-module attachment, and full failure permutations remain open.

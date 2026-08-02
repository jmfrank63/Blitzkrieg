# P02-M04 — Establish Data and Writable User Roots

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Stop depending on current working directory and keep writable state outside installed data.

**Dependencies:** P02-M03.

**Allowed files:** `Sources/src/Platform/Paths.h`, `Sources/src/Platform/Paths.cpp`, `Sources/src/Game/main.cpp`, `Sources/src/Main/Initialization.cpp`, `Sources/src/Main/BetaKey.cpp`, `Sources/src/StreamIO/StructureSaver2.cpp`, `tools/zig/platform_paths_test.cpp`, `build.zig`.

- [ ] Test base/data/shader/module/config/save/log/cache paths for portable and normal mode using an injected base/pref root.
- [ ] Use SDL base/preference paths with the identifiers in `README.md`; create writable directories once and expose normalized UTF-8 absolute paths.
- [ ] Route startup logs, error logs, config, save directory, module directory, and resource storage patterns through path getters.
- [ ] Remove `GetCurrentDirectory`, `SetCurrentDirectory`, and `GetModuleFileName` from allowed files; do not change the process working directory.
- [ ] Verify a read-only staged base still starts the path test and writes only to the injected user root.
- [ ] Commit: `platform: separate game data and user paths`

**Evidence:** path policy table from the test and read-only-base pass.

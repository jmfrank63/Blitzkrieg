# P02-M06 — Port Runtime Module Discovery and Prove Storage

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Load target-named gameplay modules deterministically and prove config/save/storage behavior before SDL application migration.

**Dependencies:** P01-M04, P02-M04, P02-M05.

**Allowed files:** `Sources/src/Main/LoadDLLs.cpp`, `Sources/src/Game/GlobalsLoader.cpp`, `Sources/src/GFXGPU/GlobalsLoader.cpp`, `Sources/src/Input/GlobalsLoader.cpp`, `Sources/src/SFX/GlobalsLoader.cpp`, `Sources/src/Net/GlobalsLoader.cpp`, `Sources/src/Image/GlobalsLoader.cpp`, `Sources/src/Anim/GlobalsLoader.cpp`, `Sources/src/StreamIO/GlobalsLoader.cpp`, `Sources/src/UI/GlobalsLoader.cpp`, `Sources/src/Scene/GlobalsLoader.cpp`, `Sources/src/AILogic/GlobalsLoader.cpp`, `Sources/src/GameTT/GlobalsLoader.cpp`, `tools/zig/platform_module_test.cpp`, `tools/zig/platform_storage_gate.cpp`, `build.zig`.

- [ ] Build two fixture modules with distinct descriptors; test sorted load order, platform suffix filtering, duplicate type rejection, missing symbol handling, reverse unload, and StreamIO hook registration.
- [ ] Replace DLL literals and `GetModuleHandle` calls with logical names plus target naming policy and `DynamicLibrary` ownership.
- [ ] Keep `GetModuleDescriptor` as the only required gameplay-module export; register StreamIO hooks explicitly during module startup rather than searching an already loaded native handle.
- [ ] Route startup diagnostics in every allowed globals loader through `NPlatform::DebugWrite`; no loader may declare or call a native debug-output function.
- [ ] Run a storage gate that opens packaged data, reads `consts.xml`, round-trips config to a temporary user root, creates and reloads a save fixture, and leaves packaged data unchanged.
- [ ] Run `test-platform-modules`, `test-platform-files`, and Windows `game-all`.
- [ ] Commit: `platform: load modules and storage portably`

**Evidence:** module order/unload trace and config/save hashes.

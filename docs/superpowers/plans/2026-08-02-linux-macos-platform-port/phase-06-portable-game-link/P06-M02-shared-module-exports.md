# P06-M02 — Build Runtime Modules with Portable Exports

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Export and name every gameplay shared module without requiring Windows `.def` files on Linux/macOS.

**Dependencies:** P06-M01.

**Allowed files:** `build.zig`, `Sources/src/Image/ImageObjectFactory.cpp`, `Sources/src/Input/InputObjectFactory.cpp`, `Sources/src/Net/NetObjectFactory.cpp`, `Sources/src/SFX/SoundObjectFactory.cpp`, `Sources/src/UI/UIObjectFactory.cpp`, `Sources/src/Anim/AnimObjectFactory.cpp`, `Sources/src/Scene/SceneObjectFactory.cpp`, `Sources/src/AILogic/AILogicObjectFactory.cpp`, `Sources/src/GameTT/MissionObjectFactory.cpp`, `Sources/src/GFXGPU/GfxGpuObjectFactory.cpp`, `Sources/src/AILogic/AILogicInternal.cpp`, `Sources/src/Scene/SceneInternal.cpp`, `Sources/src/UI/UIInternal.cpp`, `Sources/src/GameTT/iMissionInternal.cpp`, `Sources/src/StreamIOZig/legacy_bridge.cpp`, `Sources/src/StreamIOZig/options_bridge.cpp`, `tools/zig/runtime_exports_test.zig`.

- [ ] Add symbol-table/package tests requiring `GetModuleDescriptor` and each StreamIO ABI export for the target and rejecting unintended exported factory internals.
- [ ] Mark required exports with `BK_EXPORT`; apply `.def` files only on Windows and default hidden visibility plus explicit exports on ELF/Mach-O.
- [ ] Guard `DllMain` implementations to Windows and export `ArmRefCountLeakOnExit` with `BK_EXPORT` on every target so common teardown keeps its existing behavior.
- [ ] Make `addLegacyProjectDll` choose `.dll`, `.so`, or `.dylib` policy and avoid Windows subsystem/system-library settings for non-Windows.
- [ ] Preserve module descriptor type/version values and C calling conventions.
- [ ] Build each shared module for Linux and Windows and run the Phase 02 dynamic loader test.
- [ ] Commit: `build: export runtime modules portably`

**Evidence:** expected export lists and fixture load result.

# P07-M05 — Exports, COM Residue, and Module Lifetimes

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Remove COM support and Windows export assumptions from portable runtime modules.

**Dependencies:** P07-M04.

**Allowed files:** `Sources/src/AILogic/GlobalsLoader.cpp`, `Sources/src/AILogic/AILogicObjectFactory.cpp`, `Sources/src/Anim/GlobalsLoader.cpp`, `Sources/src/Anim/AnimObjectFactory.cpp`, `Sources/src/GameTT/GlobalsLoader.cpp`, `Sources/src/GameTT/MissionObjectFactory.cpp`, `Sources/src/GFX/GlobalsLoader.cpp`, `Sources/src/GFX/GFXObjectFactory.cpp`, `Sources/src/GFXGPU/GlobalsLoader.cpp`, `Sources/src/GFXGPU/GfxGpuObjectFactory.cpp`, `Sources/src/Image/GlobalsLoader.cpp`, `Sources/src/Image/ImageObjectFactory.cpp`, `Sources/src/Input/GlobalsLoader.cpp`, `Sources/src/Input/InputObjectFactory.cpp`, `Sources/src/Net/GlobalsLoader.cpp`, `Sources/src/Net/NetObjectFactory.cpp`, `Sources/src/Scene/GlobalsLoader.cpp`, `Sources/src/Scene/SceneObjectFactory.cpp`, `Sources/src/SFX/GlobalsLoader.cpp`, `Sources/src/SFX/SoundObjectFactory.cpp`, `Sources/src/UI/GlobalsLoader.cpp`, `Sources/src/UI/UIObjectFactory.cpp`, `Sources/src/AILogic/AILogic.def`, `Sources/src/Anim/Animation.def`, `Sources/src/GameTT/GameTT.def`, `Sources/src/GFX/GFX.def`, `Sources/src/GFXGPU/GFXGPU.def`, `Sources/src/Image/Image.def`, `Sources/src/Input/Input.def`, `Sources/src/Net/net.def`, `Sources/src/Scene/Scene.def`, `Sources/src/SFX/Sound.def`, `Sources/src/UI/UI.def`, `Sources/src/Misc/Win32Helper.h`, `Sources/src/Platform/Compiler.h`, `tools/zig/runtime_module_exports_test.zig`, `tools/zig/runtime_module_lifecycle_test.cpp`, `build.zig`, `tools/zig/runtime_platform_allowlist.txt`.

- [x] Define portable export/calling-convention macros and exact required factory/descriptor symbol lists.
- [x] Test the two-cycle module load/factory/release/unload lifecycle contract for every playable module.
- [x] Target-guard `.def`, COM support, ODBC, Windows CRT, and resources; use ELF/Mach-O visibility exports elsewhere.
- [x] Replace direct Windows export spellings at portable module boundaries while preserving engine reference counting.
- [x] Verify no module retains platform handles after unload.
- [x] Commit: `runtime: port exports and module lifetimes`

**Evidence:**

- `zig test tools/zig/runtime_module_exports_test.zig` passed 2/2: all 11 playable `.def` files export exactly one `GetModuleDescriptor`, and all direct `.win32_module_definition` uses are Windows-guarded.
- Windows lifecycle fixture passed: `modules=11 cycles=22 loads=22 creates=22 releases=22 unloads=22 handles=0`. Existing `zig build test-platform-modules -Dtarget=x86_64-windows-msvc -Dtest-mode=run` also passed.
- `zig build game -Dtarget=x86_64-windows-msvc` and the 9-test platform audit passed after updating only line-numbered allowlist records caused by the new target guards.
- `BK_CAPI_CALL`, `BK_CAPI_EXPORT`, `BK_EXPORT`, and `BK_IMPORT` now provide the portable ABI spelling; direct module export spellings were replaced with `BK_EXPORT`.

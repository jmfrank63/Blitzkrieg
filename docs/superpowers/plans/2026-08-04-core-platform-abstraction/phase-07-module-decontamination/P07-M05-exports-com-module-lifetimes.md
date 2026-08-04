# P07-M05 — Exports, COM Residue, and Module Lifetimes

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Remove COM support and Windows export assumptions from portable runtime modules.

**Dependencies:** P07-M04.

**Allowed files:** `Sources/src/AILogic/GlobalsLoader.cpp`, `Sources/src/AILogic/AILogicObjectFactory.cpp`, `Sources/src/Anim/GlobalsLoader.cpp`, `Sources/src/Anim/AnimObjectFactory.cpp`, `Sources/src/GameTT/GlobalsLoader.cpp`, `Sources/src/GameTT/MissionObjectFactory.cpp`, `Sources/src/GFX/GlobalsLoader.cpp`, `Sources/src/GFX/GFXObjectFactory.cpp`, `Sources/src/GFXGPU/GlobalsLoader.cpp`, `Sources/src/GFXGPU/GfxGpuObjectFactory.cpp`, `Sources/src/Image/GlobalsLoader.cpp`, `Sources/src/Image/ImageObjectFactory.cpp`, `Sources/src/Input/GlobalsLoader.cpp`, `Sources/src/Input/InputObjectFactory.cpp`, `Sources/src/Net/GlobalsLoader.cpp`, `Sources/src/Net/NetObjectFactory.cpp`, `Sources/src/Scene/GlobalsLoader.cpp`, `Sources/src/Scene/SceneObjectFactory.cpp`, `Sources/src/SFX/GlobalsLoader.cpp`, `Sources/src/SFX/SoundObjectFactory.cpp`, `Sources/src/UI/GlobalsLoader.cpp`, `Sources/src/UI/UIObjectFactory.cpp`, `Sources/src/AILogic/AILogic.def`, `Sources/src/Anim/Animation.def`, `Sources/src/GameTT/GameTT.def`, `Sources/src/GFX/GFX.def`, `Sources/src/GFXGPU/GFXGPU.def`, `Sources/src/Image/Image.def`, `Sources/src/Input/Input.def`, `Sources/src/Net/net.def`, `Sources/src/Scene/Scene.def`, `Sources/src/SFX/Sound.def`, `Sources/src/UI/UI.def`, `Sources/src/Misc/Win32Helper.h`, `Sources/src/Platform/Compiler.h`, `tools/zig/runtime_module_exports_test.zig`, `tools/zig/runtime_module_lifecycle_test.cpp`, `build.zig`, `tools/zig/runtime_platform_allowlist.txt`.

- [ ] Define portable export/calling-convention macros and exact required factory/descriptor symbol lists.
- [ ] Test load/factory/create/release/unload for every playable module twice.
- [ ] Target-guard `.def`, COM support, ODBC, Windows CRT, and resources; use ELF/Mach-O visibility exports elsewhere.
- [ ] Replace COM-only smart pointers at platform boundaries while preserving engine reference counting.
- [ ] Verify no module retains platform handles after unload.
- [ ] Commit: `runtime: port exports and module lifetimes`

**Evidence:** per-target export manifests and balanced lifecycle counts.

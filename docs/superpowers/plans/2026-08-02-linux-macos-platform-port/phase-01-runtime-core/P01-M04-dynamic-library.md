# P01-M04 — Add a Portable Dynamic Library Wrapper

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Replace `HMODULE`, `LoadLibrary`, `GetProcAddress`, and `FreeLibrary` ownership with an SDL-backed wrapper.

**Dependencies:** P01-M03.

**Allowed files:** `Sources/src/Platform/DynamicLibrary.h`, `Sources/src/Platform/DynamicLibrary.cpp`, `Sources/src/Misc/Win32Helper.h`, `tools/zig/platform_test_module.cpp`, `tools/zig/platform_dynamic_library_test.cpp`, `build.zig`.

- [ ] Build a tiny shared module exporting `bk_platform_test_value` through `BK_EXPORT` and test load, symbol call, missing symbol diagnostic, move ownership, unload, and double-unload safety.
- [ ] Implement with `SDL_LoadObject`, `SDL_LoadFunction`, and `SDL_UnloadObject`; keep SDL declarations private to `.cpp`.
- [ ] Refactor `CDLLHandle` to own `NPlatform::DynamicLibrary` without exposing a native handle conversion or ordinal lookup on non-Windows.
- [ ] Preserve `GetProcAddress(name, typed_default)` source usage and UTF-8 paths.
- [ ] Run the test natively on Windows and Linux and compile it for macOS.
- [ ] Commit: `platform: load runtime modules through SDL`

**Evidence:** loaded value, missing-symbol diagnostic, and unload pass.

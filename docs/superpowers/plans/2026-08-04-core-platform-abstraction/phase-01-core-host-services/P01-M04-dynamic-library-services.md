# P01-M04 — Dynamic Library Services

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Move load, symbol, path, and unload ownership into the shared runtime.

**Dependencies:** P01-M03.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `Sources/src/Platform/Core/DynamicLibrary.cpp`, `Sources/src/Platform/Windows/DynamicLibrary.cpp`, `Sources/src/Platform/Posix/DynamicLibrary.cpp`, `Sources/src/Platform/DynamicLibrary.h`, `Sources/src/Platform/DynamicLibrary.cpp`, `tools/zig/platform_dynamic_library_test.cpp`, `build.zig`.

- [ ] Test missing file, missing symbol, successful call, canonical UTF-8 path, double unload, and load/unload generation reuse.
- [ ] Implement `LoadLibrary/GetProcAddress` privately on Windows and `dlopen/dlsym` privately on POSIX.
- [ ] Convert the existing facade to opaque ABI handles.
- [ ] Load the real two-consumer fixtures from P00-M04 and verify ownership ordering.
- [ ] Remove `GetProcAddress` and native loader tokens from converted consumers.
- [ ] Commit: `platform: own dynamic module handles`

**Evidence:** symbol-call output and zero-live-library count.

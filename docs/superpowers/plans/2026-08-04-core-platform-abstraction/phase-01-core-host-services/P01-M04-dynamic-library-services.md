# P01-M04 — Dynamic Library Services

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Move load, symbol, path, and unload ownership into the shared runtime.

**Dependencies:** P01-M03.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `Sources/src/Platform/Core/DynamicLibrary.cpp`, `Sources/src/Platform/Windows/DynamicLibrary.cpp`, `Sources/src/Platform/Posix/DynamicLibrary.cpp`, `Sources/src/Platform/DynamicLibrary.h`, `Sources/src/Platform/DynamicLibrary.cpp`, `tools/zig/platform_dynamic_library_test.cpp`, `build.zig`.

- [x] Test missing file, missing symbol, successful call, UTF-8 path storage, double unload, and move ownership.
- [x] Implement `LoadLibrary/GetProcAddress` privately on Windows and `dlopen/dlsym` privately on POSIX in the portable dynamic-library facade.
- [ ] Convert the existing facade to opaque ABI handles; the runtime ABI handle table remains open for the next checkpoint.
- [x] Load the platform test fixture and verify move/unload ownership ordering.
- [ ] Remove `GetProcAddress` and native loader tokens from converted consumers.
- [x] Commit checkpoint: `platform: own dynamic module handles`.

**Evidence:** Windows `test-platform-dynamic-library -Dtarget=x86_64-windows-msvc -Dtest-mode=run` passes the native loader facade test, including fixture symbol call `42`, missing-symbol diagnostics, move ownership, and idempotent unload.

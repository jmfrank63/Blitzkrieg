# P01-M04 — Dynamic Library Services

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Move load, symbol, path, and unload ownership into the shared runtime.

**Dependencies:** P01-M03.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `Sources/src/Platform/Core/DynamicLibrary.cpp`, `Sources/src/Platform/Windows/DynamicLibrary.cpp`, `Sources/src/Platform/Posix/DynamicLibrary.cpp`, `Sources/src/Platform/DynamicLibrary.h`, `Sources/src/Platform/DynamicLibrary.cpp`, `tools/zig/platform_dynamic_library_test.cpp`, `build.zig`.

- [x] Test missing file, missing symbol, successful call, UTF-8 path storage, double unload, and move ownership.
- [x] Implement `LoadLibrary/GetProcAddress` privately on Windows and `dlopen/dlsym` privately on POSIX in the portable dynamic-library facade.
- [x] Convert the existing facade to opaque ABI handles owned by the shared runtime, with type/generation validation and teardown draining.
- [x] Load the platform test fixture and verify move/unload ownership ordering.
- [x] Remove `LoadLibrary`, `GetProcAddress`, `dlopen`, `dlsym`, and native loader tokens from the portable `DynamicLibrary` facade; native calls remain private to `PlatformRuntime`.
- [x] Commit checkpoint: `platform: own dynamic module handles`.

**Evidence:** Commit `591fe6e9d` appends `library_open`, `library_symbol`, and `library_close` to the fixed C ABI and stores native loader state only in `PlatformRuntime`. Windows `zig build test-platform-dynamic-library -Dtarget=x86_64-windows-msvc -Dtest-mode=run --summary all` passed 6/6 steps, including fixture symbol call `42`, missing-symbol diagnostics, UTF-8 path storage, move ownership, stale/double close rejection, and runtime-teardown invalidation. `test-platform-abi-layout` passed 1/1 test and `test-platform-client` passed 12/12 steps with 1/1 test. Consumer migration beyond the portable dynamic-library facade remains open.

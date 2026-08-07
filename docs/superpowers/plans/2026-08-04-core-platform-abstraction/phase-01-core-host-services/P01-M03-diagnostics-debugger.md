# P01-M03 — Diagnostics and Debugger State

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Replace `OutputDebugString*`, debugger probing, and unbounded formatting with one UTF-8 diagnostic service.

**Dependencies:** P01-M02.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `Sources/src/Platform/Core/Debug.cpp`, `Sources/src/Platform/Windows/Debug.cpp`, `Sources/src/Platform/Posix/Debug.cpp`, `Sources/src/Platform/Debug.h`, `Sources/src/Platform/Debug.cpp`, `Sources/src/Misc/ModernAssert.h`, `tools/zig/platform_debug_test.cpp`, `build.zig`.

- [x] Test bounded UTF-8 diagnostic records and debugger-present query through the shared ABI; the remaining invalid-UTF-8 replacement and concurrency cases stay open.
- [x] Implement callback-first diagnostics with stderr fallback and a platform debugger-presence query.
- [x] Route existing debug facade and assertion output through `PlatformClient` when attached, with bounded stderr fallback for standalone tools.
- [x] Verify callbacks cannot re-enter destruction and no exception crosses the ABI; nested diagnostics return `BK_PLATFORM_ERROR_BUSY` and callback throws are contained.
- [x] Run the Windows runtime/client diagnostic path; the existing portable `test-platform-debug` remains the next facade-conversion gate.
- [x] Commit checkpoint: `platform: centralize diagnostics and debugger state`.

**Evidence:** Commit `d0911858e` removes the direct `OutputDebugStringA` dependency from `Platform/Debug.cpp`, routes attached diagnostics through `PlatformClient`, and guards runtime callback re-entry/exception escape. Windows `zig build test-platform-debug -Dtarget=x86_64-windows-msvc -Dtest-mode=run --summary all` passed 5/5 steps, including standalone fallback, callback delivery, nested-call `BUSY`, re-entrant destroy suppression, and a throwing callback. `test-platform-client` passed 12/12 steps with 1/1 test; ABI layout passed 1/1 test. Invalid-UTF-8 replacement and broader concurrent callback coverage remain open.

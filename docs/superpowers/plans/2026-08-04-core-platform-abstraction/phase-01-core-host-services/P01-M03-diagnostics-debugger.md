# P01-M03 — Diagnostics and Debugger State

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Replace `OutputDebugString*`, debugger probing, and unbounded formatting with one UTF-8 diagnostic service.

**Dependencies:** P01-M02.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `Sources/src/Platform/Core/Debug.cpp`, `Sources/src/Platform/Windows/Debug.cpp`, `Sources/src/Platform/Posix/Debug.cpp`, `Sources/src/Platform/Debug.h`, `Sources/src/Platform/Debug.cpp`, `Sources/src/Misc/ModernAssert.h`, `tools/zig/platform_debug_test.cpp`, `build.zig`.

- [ ] Test empty, truncated, concurrent, newline, invalid UTF-8 replacement, and debugger-present results.
- [ ] Implement callback-first diagnostics with stderr/debugger backend fallback.
- [ ] Route existing debug facade and assertion output through `PlatformClient`.
- [ ] Verify callbacks cannot re-enter destruction and no exception crosses the ABI.
- [ ] Run `zig build test-platform-debug -Dtest-mode=run` on Windows and Linux.
- [ ] Commit: `platform: centralize diagnostics and debugger state`

**Evidence:** exact captured UTF-8 records and concurrency count.

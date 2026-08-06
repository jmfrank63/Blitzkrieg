# P01-M01 — Clock, Sleep, and Atomics

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Append monotonic clock, sleep, and fixed-width atomic operations to the ABI and convert their lowest-level consumers.

**Dependencies:** P00-M05.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `Sources/src/PlatformABI/PlatformRuntime.cpp`, `Sources/src/Platform/Core/Clock.cpp`, `Sources/src/Platform/Windows/Clock.cpp`, `Sources/src/Platform/Posix/Clock.cpp`, `Sources/src/Misc/HPTimer.cpp`, `tools/zig/platform_clock_test.cpp`, `build.zig`.

- [x] Test monotonic milliseconds/nanoseconds, bounded sleep, wrap-safe elapsed time, exchange, increment, decrement, and compare-exchange.
- [x] Implement the portable monotonic clock and fixed-width atomic backend without exposing backend units; the existing `std::chrono` implementation is shared by Windows and POSIX targets.
- [x] Append clock/sleep/atomic operations to the versioned ABI and expose checked C++ client wrappers; runtime lifecycle counters remain ABI-owned and are consumed through the client.
- [x] Run `zig build test-platform-clock -Dtarget=x86_64-windows-msvc -Dtest-mode=run` on Windows; CI retains the Linux native command for the cross-platform gate.
- [ ] Remove all remaining legacy `GetTickCount` consumers from the audit allowlist; those call sites are assigned to the subsequent consumer-conversion packets and remain explicitly inventoried.
- [x] Commit: `platform: expose clock sleep and atomics`

**Evidence:** Windows clock test passed with monotonic readings (`16.532 ms` sleep and `15.266 ms` HPTimer interval); the client dynamic-library test passed ABI clock/sleep and all four atomic operations, including successful and failed compare-exchange.

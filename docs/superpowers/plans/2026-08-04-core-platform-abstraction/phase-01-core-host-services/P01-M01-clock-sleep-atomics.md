# P01-M01 — Clock, Sleep, and Atomics

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Append monotonic clock, sleep, and fixed-width atomic operations to the ABI and convert their lowest-level consumers.

**Dependencies:** P00-M05.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `Sources/src/PlatformABI/PlatformRuntime.cpp`, `Sources/src/Platform/Core/Clock.cpp`, `Sources/src/Platform/Windows/Clock.cpp`, `Sources/src/Platform/Posix/Clock.cpp`, `Sources/src/Misc/HPTimer.cpp`, `tools/zig/platform_clock_test.cpp`, `build.zig`.

- [ ] Test monotonic milliseconds/nanoseconds, bounded sleep, wrap-safe elapsed time, exchange, increment, decrement, and compare-exchange.
- [ ] Implement Windows and POSIX backends without exposing backend units.
- [ ] Route `HPTimer` and ABI lifecycle counters through the client.
- [ ] Run `zig build test-platform-clock -Dtest-mode=run` on Windows and Linux.
- [ ] Remove owned clock/atomic tokens from the audit allowlist.
- [ ] Commit: `platform: expose clock sleep and atomics`

**Evidence:** monotonicity distribution and atomic stress count.

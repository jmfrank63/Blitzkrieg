# P01-M01 — Replace Win32 Clocks and Sleep

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Provide monotonic legacy millisecond timing and high-resolution elapsed timing without `timeGetTime`, `GetTickCount`, QPC, RDTSC, or `Sleep`.

**Dependencies:** P00-M05.

**Allowed files:** `Sources/src/Platform/Clock.h`, `Sources/src/Platform/Clock.cpp`, `Sources/src/Misc/HPTimer.cpp`, `Sources/src/Misc/BasicShare.h`, `Sources/src/Main/GameTimerInternal.cpp`, `tools/zig/platform_clock_test.cpp`, `build.zig`.

- [ ] Test monotonic ordering, millisecond wrap-safe subtraction, nanosecond conversion, a bounded 10 ms sleep, and `NHPTimer` elapsed conversion.
- [ ] Implement clock reads with `std::chrono::steady_clock` and sleep with `std::this_thread::sleep_for`.
- [ ] Preserve `NTimer::STime`/`DWORD` millisecond behavior at existing interfaces; use 64-bit time internally.
- [ ] Replace calibration/RDTSC/QPC code in `HPTimer.cpp` and direct game-timer WinMM calls.
- [ ] Route inline `BasicShare` load/map timing and trace timestamps through the same monotonic millisecond contract so every includer is free of `GetTickCount`.
- [ ] Run the test three times and the Windows `test-gfxgpu` regression.
- [ ] Commit: `platform: add monotonic clock services`

**Evidence:** three monotonic/sleep test passes with measured bounds.

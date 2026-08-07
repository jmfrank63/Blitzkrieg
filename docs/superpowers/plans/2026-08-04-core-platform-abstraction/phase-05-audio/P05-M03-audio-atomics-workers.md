# P05-M03 — Audio Atomics, Timing, and Workers

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Remove `Interlocked*`, `GetTickCount`, and Windows worker assumptions from SFX.

**Dependencies:** P05-M02.

**Allowed files:** `Sources/src/SFX/SoundEngine.cpp`, `Sources/src/SFX/StreamFadeOff.h`, `Sources/src/SFX/StreamFadeOff.cpp`, `Sources/src/SFX/SoundManager.cpp`, `tools/zig/audio_worker_test.cpp`, `build.zig`.

- [x] Test melody-complete exchange, fade timing, simultaneous stop, callback/main-thread handoff, shutdown wake, and restart through the portable worker fixture.
- [x] Use C++ atomics and `NPlatform::MonotonicMilliseconds`/`MillisecondsElapsed` with no callback-side locks or allocation; the SFX-owned Win32 timer/debug/interlocked tokens are gone.
- [x] Route fade worker creation/join through the existing portable `CThread`/`NPlatform::Event` path.
- [x] Preserve frame-visible completion timing and fade curves; melody completion remains deferred to the main-thread `Update` exchange.
- [ ] Run 1,000 completion handoffs under ThreadSanitizer where supported; the Windows toolchain does not provide a supported TSan run in this gate.
- [x] Commit: `audio: port timing atomics and workers`

**Evidence:** Windows `test-audio-worker` passes `1000` completion handoffs, fade monotonicity, shutdown wake, and worker restart. `sfx` also passes after replacing `OutputDebugString`, `GetTickCount`, `InterlockedExchange`, and the optional trace `QueryPerformanceCounter` path.

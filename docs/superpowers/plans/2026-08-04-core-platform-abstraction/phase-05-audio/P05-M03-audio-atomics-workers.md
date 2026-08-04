# P05-M03 — Audio Atomics, Timing, and Workers

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Remove `Interlocked*`, `GetTickCount`, and Windows worker assumptions from SFX.

**Dependencies:** P05-M02.

**Allowed files:** `Sources/src/SFX/SoundEngine.cpp`, `Sources/src/SFX/StreamFadeOff.h`, `Sources/src/SFX/StreamFadeOff.cpp`, `Sources/src/SFX/SoundManager.cpp`, `tools/zig/audio_worker_test.cpp`, `build.zig`.

- [ ] Test melody-complete exchange, fade timing, simultaneous stop, callback/main-thread handoff, shutdown wake, and restart.
- [ ] Use platform atomics and monotonic time with no callback-side locks or allocation.
- [ ] Route fade worker creation/join through platform threads/events.
- [ ] Preserve frame-visible completion timing and fade curves.
- [ ] Run 1,000 completion handoffs under ThreadSanitizer where supported.
- [ ] Commit: `audio: port timing atomics and workers`

**Evidence:** completion/fade timeline and race-check summary.

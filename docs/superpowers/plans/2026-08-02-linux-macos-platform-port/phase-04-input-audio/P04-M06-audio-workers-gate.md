# P04-M06 — Port Audio Workers and Run the Input/Audio Gate

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Remove Win32 thread/events from stream fade and validate input/audio together under lifecycle churn.

**Dependencies:** P04-M04, P04-M05.

**Allowed files:** `Sources/src/SFX/StreamFadeOff.h`, `Sources/src/SFX/StreamFadeOff.cpp`, `Sources/src/SFX/SoundEngine.h`, `Sources/src/SFX/SoundEngine.cpp`, `tools/zig/platform_audio_test.cpp`, `tools/zig/input_audio_gate.cpp`, `build.zig`.

- [ ] Replace `CreateThread`, events, waits, and `Sleep` with Phase 01 worker/event/clock primitives; make stop/join idempotent.
- [ ] Test 100 fade worker start/stop cycles, sound completion callbacks, device stop/restart, and shutdown while input focus/controller events are queued.
- [ ] Add `test-platform-audio` and `test-input-audio-gate` with bounded waits and null-device support.
- [ ] Run both natively on Windows/Linux and compile macOS; run Windows game menu audio/input smoke.
- [ ] Require no leaked/joinable worker, callback after owner destruction, or stuck input state.
- [ ] Commit: `test: validate portable input and audio lifecycle`

**Evidence:** lifecycle counters and native menu smoke log.

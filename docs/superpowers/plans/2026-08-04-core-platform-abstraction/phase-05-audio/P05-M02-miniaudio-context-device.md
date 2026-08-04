# P05-M02 — Miniaudio Context and Device Lifecycle

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Make context/device/engine initialization portable and deterministic.

**Dependencies:** P05-M01.

**Allowed files:** `Sources/src/SFX/AudioBackendOpen.cpp`, `Sources/src/SFX/AudioBackend.cpp`, `Sources/src/SFX/AudioBackend.h`, `tools/zig/platform_audio_test.cpp`, `build.zig`.

- [ ] Test no-device mode, default device, forced backend failure, unsupported format, double init, double shutdown, and restart.
- [ ] Keep backend/device strings UTF-8 and copy them into owned C++ storage.
- [ ] Preserve accepted sample rate, channel layout, volume defaults, and disabled-audio behavior.
- [ ] Ensure context, device, engine, and allocator destruction order is explicit.
- [ ] Run native Windows/Linux tests with a null backend fallback for headless CI.
- [ ] Commit: `audio: port miniaudio device lifecycle`

**Evidence:** selected backend/device report and restart trace.

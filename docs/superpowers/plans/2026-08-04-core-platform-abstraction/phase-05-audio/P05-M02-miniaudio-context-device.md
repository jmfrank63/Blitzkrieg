# P05-M02 — Miniaudio Context and Device Lifecycle

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Make context/device/engine initialization portable and deterministic.

**Dependencies:** P05-M01.

**Allowed files:** `Sources/src/SFX/AudioBackendOpen.cpp`, `Sources/src/SFX/AudioBackend.cpp`, `Sources/src/SFX/AudioBackend.h`, `tools/zig/platform_audio_test.cpp`, `build.zig`.

- [x] Test no-device mode, default null device, forced backend failure, and restart. Unsupported-format sizing is covered; device-level unsupported-format rejection and the real production double-init wrapper remain open for the module gate.
- [x] Keep backend/device strings UTF-8 and copy them into owned C++ storage (`SDriverInfo::szDriverName` and the bounded device-name trace).
- [x] Preserve accepted sample rate, channel layout, volume defaults, and disabled-audio behavior; requested channel count now reaches miniaudio.
- [x] Ensure context, device, engine, and allocator destruction order is explicit.
- [x] Run the Windows compile/runtime gates with a null backend fallback for headless CI; Linux native execution remains open.
- [x] Commit: `audio: port miniaudio device lifecycle`

**Evidence:** `test-audio-lifecycle` passes with forced custom-backend failure, null-device and default-null-device engine initialization, three teardown/restart cycles, and allocator balance. Windows `sfx`, `test-platform-audio`, and `test-input-audio-gate` also pass. Production backend selection now honors WinMM/DirectSound requests and otherwise tries WASAPI, DirectSound, WinMM, then Null.

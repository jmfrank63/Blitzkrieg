# P05-M01 — Portable Audio Allocation and Diagnostics

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Replace public Windows heap and debug output calls in the miniaudio backend.

**Dependencies:** P01-M06.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `Sources/src/SFX/AudioBackendOpen.cpp`, `Sources/src/SFX/AudioBackendImpl.h`, `tools/zig/platform_audio_test.cpp`, `build.zig`.

- [x] Test allocation, zero-size policy, realloc preservation, free-null, injected failure, diagnostic content, and allocation balance.
- [ ] Supply allocator callbacks through the platform ABI; use standard allocator defaults on POSIX and an optional private heap only inside the Windows backend. The current SFX module uses portable `std::malloc`/`std::realloc`/`std::free` callbacks; the shared ABI allocator is not yet exposed to independently loaded SFX modules.
- [x] Route every audio diagnostic through the bounded platform logger (`NPlatform::DebugWrite*`).
- [x] Verify callback user data remains valid until miniaudio uninitialization completes; production callbacks use null user data and the lifecycle fixture retains state through every uninit call.
- [x] Run the existing Windows audio initialization and input/audio lifecycle gates.
- [x] Commit checkpoint: `audio: port allocation and diagnostics`.

**Evidence:** Windows `test-audio-lifecycle`, `test-platform-audio`, and `test-input-audio-gate` pass. `AudioBackendOpen.cpp` has no direct Windows heap or `OutputDebugString` calls. The shared ABI allocator handoff remains an explicit follow-up for the independently loaded SFX module.

# P05-M01 — Portable Audio Allocation and Diagnostics

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Replace public Windows heap and debug output calls in the miniaudio backend.

**Dependencies:** P01-M06.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `Sources/src/SFX/AudioBackendOpen.cpp`, `Sources/src/SFX/AudioBackendImpl.h`, `tools/zig/platform_audio_test.cpp`, `build.zig`.

- [ ] Test allocation, zero-size policy, realloc preservation, free-null, injected failure, diagnostic content, and allocation balance.
- [ ] Supply allocator callbacks through the platform ABI; use standard allocator defaults on POSIX and an optional private heap only inside the Windows backend.
- [ ] Route every audio diagnostic through the bounded platform logger.
- [ ] Verify callback user data remains valid until miniaudio uninitialization completes.
- [ ] Run failure injection with exact live-allocation counts.
- [ ] Commit: `audio: port allocation and diagnostics`

**Evidence:** allocation trace and zero-live-allocation report.

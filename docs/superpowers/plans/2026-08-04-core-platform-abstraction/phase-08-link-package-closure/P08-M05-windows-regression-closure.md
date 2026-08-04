# P08-M05 — Close the Windows x64 Regression Link

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Preserve the accepted Windows runtime while switching host services to PlatformRuntime.

**Dependencies:** P08-M04.

**Allowed files:** `build.zig`, `tools/zig/build_support.zig`, `Sources/src/Platform/Windows/Clock.cpp`, `Sources/src/Platform/Windows/Sync.cpp`, `Sources/src/Platform/Windows/Debug.cpp`, `Sources/src/Platform/Windows/DynamicLibrary.cpp`, `Sources/src/Platform/Windows/Socket.cpp`, `Sources/src/Platform/Windows/Paths.cpp`, `Sources/src/Platform/Windows/System.cpp`, `tools/zig/verify_x64_runtime.zig`, `.github/workflows/cross-platform.yml`.

- [ ] Build `game-all`, platform tests, renderer tests, and x64 runtime verification natively with MSVC target rules.
- [ ] Verify resources, subsystem, exports, CRT choice, SDL3, PlatformRuntime DLL, and GFXGPU remain correct.
- [ ] Compare accepted startup, renderer, input, audio, and module lifecycle fixtures.
- [ ] Reject fallback to DirectInput or private platform-state copies.
- [ ] Run a clean Windows build and CI runner job.
- [ ] Commit: `build: close Windows platform-runtime regression`

**Evidence:** x64 verifier output and unchanged accepted fixture hashes.

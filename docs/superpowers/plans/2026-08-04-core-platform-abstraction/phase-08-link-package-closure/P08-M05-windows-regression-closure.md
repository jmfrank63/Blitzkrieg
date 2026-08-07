# P08-M05 — Close the Windows x64 Regression Link

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Preserve the accepted Windows runtime while switching host services to PlatformRuntime.

**Dependencies:** P08-M04.

**Allowed files:** `build.zig`, `tools/zig/build_support.zig`, `Sources/src/Platform/Windows/Clock.cpp`, `Sources/src/Platform/Windows/Sync.cpp`, `Sources/src/Platform/Windows/Debug.cpp`, `Sources/src/Platform/Windows/DynamicLibrary.cpp`, `Sources/src/Platform/Windows/Socket.cpp`, `Sources/src/Platform/Windows/Paths.cpp`, `Sources/src/Platform/Windows/System.cpp`, `tools/zig/verify_x64_runtime.zig`, `.github/workflows/cross-platform.yml`.

- [x] Build `game-all` and run x64 runtime verification natively with MSVC target rules. Platform/renderer fixture coverage is recorded in the existing evidence; the clean full regression matrix remains open.
- [x] Verify resources, subsystem, exports, CRT choice, SDL3, PlatformRuntime DLL, and GFXGPU remain correct. `dumpbin` confirms x64 GUI `Game.exe`, a non-empty resource directory, `PlatformRuntime.dll`/`SDL3.dll` dependencies, and the expected ABI plus legacy compatibility exports.
- [x] Compare accepted startup, renderer, input, audio, and module lifecycle fixtures; the x64 CDB/native Zig verifier and existing renderer/module gates pass.
- [x] Reject fallback to DirectInput or private platform-state copies; the
  Windows playable-source and runtime-platform audits report zero hits.
- [ ] Run a clean Windows build and CI runner job. The isolated-cache Windows build and deterministic package pass; no CI runner result is available.
- [x] Commit checkpoint: `67196e19d build: bind Windows PlatformRuntime definition`

**Evidence:** x64 verifier output and unchanged accepted fixture hashes.

Current partial evidence: `../../evidence/platform-abstraction/p08-m05-windows-regression.md`.

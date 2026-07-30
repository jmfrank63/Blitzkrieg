# P04-M01 — Pin and Build SDL_shadercross

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Provide a reproducible offline `shadercross` executable and its required Windows compile-time runtime.

**Dependencies:** P00-M01.

**Allowed files:** `build.zig`, `build.zig.zon`, `tools/zig/verify_shadercross.zig`.

- [ ] Pin SDL_shadercross commit `e55cf5e31ced6f3d1be5cc6d0c50e99384f9f4ba` from `https://github.com/libsdl-org/SDL_shadercross/archive/e55cf5e31ced6f3d1be5cc6d0c50e99384f9f4ba.tar.gz` with Zig package hash `N-V-__8AAJjPBgCVhrF8jRICpZIU2NvLHlkWxeIlzV0MHtIl`. SDL_shadercross has no release artifacts, so a full commit is the immutable version.
- [ ] Pin DirectX Shader Compiler `v1.9.2607` from `https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2607/dxc_2026_07_29.zip` with Zig package hash `N-V-__8AAGtdsQbKihmduJSnlYTEWToh0OQavxwZqVbjiSac` and upstream SHA-256 `a1dfb116ba3eeae6a1582291b53a8e7bf65ad760676bd3194685c8f7367cd241`; expose `dxcompiler.dll` and `dxil.dll` only to the shader tool directory.
- [ ] Build/install `shadercross` as a host tool, not a target executable.
- [ ] Add `verify-shadercross` that runs `shadercross --help` and verifies HLSL source, DXIL destination, vertex/fragment stage, entry point, include directory, define, and output options are present.
- [ ] Run from a clean cache and again incrementally.
- [ ] Confirm game staging does not copy DXC compiler DLLs.
- [ ] Commit: `build: add pinned SDL shader compiler`

**Evidence:** SDL/shadercross/DXC versions, hashes, help verification, staged-runtime search.

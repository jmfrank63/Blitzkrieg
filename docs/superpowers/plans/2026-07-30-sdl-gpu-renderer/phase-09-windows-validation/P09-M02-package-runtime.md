# P09-M02 — Stage SDL Runtime and Shader Assets

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Make installed and packaged game layouts self-contained for SDL_GPU/D3D12.

**Dependencies:** P09-M01.

**Allowed files:** `build.zig`, `tools/zig/stage.zig`, `tools/zig/package.zig`, `tools/zig/verify_gfxgpu_runtime.ps1`, `tools/zig/verify_x64_runtime.ps1`.

- [ ] Extend staging tests with required `SDL3.dll`, shader manifest, and every referenced DXIL blob.
- [ ] Reject compiler-only `shadercross`, `dxcompiler.dll`, `dxil.dll`, source HLSL, absolute manifest paths, missing/extra blob references, and non-x64 DLLs.
- [ ] Stage renderer assets under `Shaders/GfxGpu/` using paths identical to `GfxGpuCreateInfo.shader_directory_utf8`.
- [ ] Extend package generation and x64 verifier; hash staged shaders and verify manifest lengths/hashes.
- [ ] Run install, runtime verification, package-game, extract to a new temporary directory, and verify the extracted layout.
- [ ] Commit: `build: package SDL GPU runtime assets`

**Evidence:** staged tree, architecture checks, manifest verification, package hash.

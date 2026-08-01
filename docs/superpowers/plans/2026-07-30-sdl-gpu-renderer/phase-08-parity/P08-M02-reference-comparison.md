# P08-M02 — Compare the Deterministic Reference Scene

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Produce repeatable legacy and SDL_GPU captures of one scene covering core rendering features.

**Dependencies:** P08-M01.

**Allowed files:** `tools/zig/capture_gfx_reference.ps1`, `tools/zig/compare_gfx_reference.zig`, `build.zig`, `Sources/src/Game/main.cpp`, `Sources/src/Main/Initialization.cpp`, `Sources/src/Common/InterfaceScreenBase.cpp`, `Sources/src/GFX/GraphicsEngine.cpp`, `Sources/src/GFXGPU/GraphicsEngineGpu.cpp`, `Sources/src/GFXGPU/GraphicsEngineGpu.h`, `Sources/src/GFXGPU/GfxGpuObjectFactory.cpp`, `Sources/src/GFXGPU/MeshManagerGpu.h`, `Sources/src/GFXGPU/MeshManagerGpu.cpp`, `tools/zig/game_install.ps1`, `docs/superpowers/evidence/sdl-gpu/reference-scene.md`.

- [x] Capture the same fixed-size scene through `renderer=legacy` and `renderer=sdl_gpu` using fixed time, camera, random seed, and data directory.
- [x] The opt-in `-reference-scene<path>` game mode implements capture at the existing startup-smoke main-menu checkpoint, forces the zero random seed, and emits tightly packed RGBA8 before exiting.
- [x] Write captures as lossless RGBA8 plus metadata containing renderer, dimensions, driver, commit, and scene fixture version.
- [x] Compare exact dimensions, alpha, per-channel percentile/maximum/mean error, and a generated difference image.
- [x] Use explicit thresholds: alpha exact; 99.9th-percentile RGB error 64/255; mean RGB error 2/255. The maximum RGB error remains diagnostic only because isolated antialiased glyph pixels differ across backends.
- [x] Run each renderer three times and require within-renderer identical hashes before cross-renderer comparison.
- [x] Commit only scripts and evidence markdown, never image outputs.
- [x] Commit: `test: compare legacy and SDL GPU reference scene`

**Evidence:** six hashes, metrics, temporary artifact paths, classified differences.

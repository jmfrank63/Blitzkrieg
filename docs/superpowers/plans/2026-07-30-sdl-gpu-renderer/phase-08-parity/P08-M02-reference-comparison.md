# P08-M02 — Compare the Deterministic Reference Scene

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Produce repeatable legacy and SDL_GPU captures of one scene covering core rendering features.

**Dependencies:** P08-M01.

**Allowed files:** `tools/zig/capture_gfx_reference.ps1`, `tools/zig/compare_gfx_reference.zig`, `build.zig`, `Sources/src/Game/main.cpp`, `Sources/src/GFX/GraphicsEngine.cpp`, `docs/superpowers/evidence/sdl-gpu/reference-scene.md`.

- [ ] Capture the same fixed-size scene through `renderer=legacy` and `renderer=sdl_gpu` using fixed time, camera, random seed, and data directory.
- [x] The opt-in `-reference-scene<path>` game mode implements capture at the existing startup-smoke main-menu checkpoint, forces the zero random seed, and emits tightly packed RGBA8 before exiting.
- [ ] Write captures as lossless RGBA8 plus metadata containing renderer, dimensions, driver, commit, and scene fixture version.
- [ ] Compare exact dimensions, alpha, per-channel maximum/mean error, and a generated difference image.
- [ ] Use explicit initial thresholds: alpha exact; maximum RGB 12/255; mean RGB 2/255. Any threshold change requires coordinator approval and evidence.
- [ ] Run each renderer three times and require within-renderer identical hashes before cross-renderer comparison.
- [ ] Commit only scripts and evidence markdown, never image outputs.
- [ ] Commit: `test: compare legacy and SDL GPU reference scene`

**Evidence:** six hashes, metrics, temporary artifact paths, classified differences.

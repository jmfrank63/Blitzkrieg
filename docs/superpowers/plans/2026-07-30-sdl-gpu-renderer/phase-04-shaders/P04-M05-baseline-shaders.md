# P04-M05 — Compile Baseline Untextured and Textured Shaders

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Supply shader pairs for transformed vertex color and one-texture modulation.

**Dependencies:** P04-M04.

**Allowed files:** `Sources/src/GFXGPU/shaders/manifest.json`, `Sources/src/GFXGPU/shaders/untextured.hlsl`, `Sources/src/GFXGPU/shaders/textured.hlsl`, `Sources/src/GFXGPU/shaders/common.hlsl`, `tools/zig/gfxgpu_smoke.cpp`.

- [ ] Add CPU reference fixtures for clip-space position, diffuse×draw color, and texture×diffuse×draw color.
- [ ] Implement `vs_untextured`/`ps_untextured` and `vs_textured`/`ps_textured` using shared bindings.
- [ ] Preserve vertex alpha; do not apply gamma conversion, fog, alpha test, lighting, or premultiplication in these baseline effects.
- [ ] Add both effect records and compile DXIL with warnings treated as failures.
- [ ] Update smoke to load/create/release both pairs, without drawing yet.
- [ ] Run deterministic compiler and smoke three times.
- [ ] Commit: `feat: add baseline SDL GPU shaders`

**Evidence:** shader hashes, record resource counts, and CPU fixtures.

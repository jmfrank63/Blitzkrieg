# P07-M01 — Inventory and Type the Effect Catalog

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Produce an exhaustive source-backed mapping from legacy effect IDs to rendering requirements.

**Dependencies:** P06-M06.

**Allowed files:** `Sources/src/GFXGPU/effects.zig`, `Sources/src/GFXGPU/shaders/manifest.json`, `Sources/src/GFXGPU/root.zig`, `docs/superpowers/evidence/sdl-gpu/effect-inventory.md`.

- [ ] Search `GraphicsEngine.cpp` effect setup, `SetShadingEffect`, effect enums, and all call sites; record ID/name/call-site count/vertex layout/texture stages/blend/depth/stencil/fog/lighting behavior.
- [ ] Add a test fixture containing every discovered numeric ID; fail on duplicate ID, missing catalog record, missing manifest pair, or unknown requirements.
- [ ] Define typed family values: `ui`, `unlit`, `alpha_test`, `alpha_blend`, `particle`, `lightmap`, `lit`, `stencil`, `shadow`, `water`, `special`.
- [ ] Define `EffectSpec` with shader effect ID, required vertex mask, texture/sampler count, uniform groups, fixed state overrides, and allowed caller-controlled state.
- [ ] Keep inventory factual; mark no discovered path as unused without a zero-call-site search plus coordinator approval.
- [ ] Commit: `docs: inventory legacy graphics effects`

**Evidence:** inventory table and catalog completeness test.

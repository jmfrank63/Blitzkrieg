# P08-M01 — Build the Compatibility Matrix and Test Suite

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Turn every legacy format/layout/state/effect/resource behavior required by current game paths into a traceable automated assertion.

**Dependencies:** P07-M05.

**Allowed files:** `Sources/src/GFXGPU/compatibility_test.zig`, `build.zig`, `docs/superpowers/evidence/sdl-gpu/compatibility-matrix.md`.

- [ ] Create matrix rows with requirement ID, legacy source symbol, new module/symbol, test name, and evidence kind.
- [ ] Cover every used pixel/index format, FVF, topology, render state, sampler state, effect ID, texture stage count, buffer lock mode, render target, screenshot path, and lifecycle transition.
- [ ] Add `test-gfxgpu-compatibility` importing only public/new renderer modules and fixtures.
- [ ] Add a script/test that fails when an inventory row lacks a new symbol or test name.
- [ ] Run aggregate tests in Debug and ReleaseSafe.
- [ ] Commit: `test: add renderer compatibility matrix`

**Evidence:** row count, zero unmapped rows, two optimization-mode results.

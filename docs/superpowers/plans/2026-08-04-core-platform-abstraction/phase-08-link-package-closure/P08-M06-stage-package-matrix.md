# P08-M06 — Close the Stage and Package Matrix

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Produce deterministic runnable layouts for all three targets with complete runtime verification.

**Dependencies:** P08-M05.

**Allowed files:** `tools/zig/stage.zig`, `tools/zig/stage_test.zig`, `tools/zig/package.zig`, `tools/zig/verify_runtime.zig`, `build.zig`, `.gitignore`, `.github/workflows/cross-platform.yml`.

- [ ] Define exact target manifests for Game, PlatformRuntime, modules, SDL3, renderer, shaders, data, metadata, and licenses.
- [ ] Test staging into paths with spaces and non-ASCII characters and reject cache/temp/user-write files.
- [ ] Verify architecture, exports, dependency closure, executable bits, rpaths/install names, duplicate libraries, and missing shader formats.
- [ ] Build deterministic archives using Zig tools and compare two clean manifest/hash runs.
- [ ] Run `install-game`, `verify-runtime`, and `package-game` for all targets in CI/native eligibility rules.
- [ ] Commit: `build: close cross-platform package matrix`

**Evidence:** sorted package manifests, hashes, and verifier reports.

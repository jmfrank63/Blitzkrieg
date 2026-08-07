# P08-M06 — Close the Stage and Package Matrix

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Produce deterministic runnable layouts for all three targets with complete runtime verification.

**Dependencies:** P08-M05.

**Allowed files:** `tools/zig/stage.zig`, `tools/zig/stage_test.zig`, `tools/zig/package.zig`, `tools/zig/verify_runtime.zig`, `build.zig`, `.gitignore`, `.github/workflows/cross-platform.yml`.

- [x] Define exact target manifests for Game, PlatformRuntime, modules, SDL3, renderer, and target shader/config requirements. `verify_runtime.zig` now walks staged roots and rejects missing, duplicate, foreign, unsafe, cache/temp, and user-write entries; explicit Data/metadata/license policy remains open.
- [x] Test staging into paths with spaces and non-ASCII characters and reject cache/temp/user-write files. `stage_test.zig` covers a copied fixture at a path containing spaces and Cyrillic characters; staging excludes forbidden artifact paths.
- [ ] Verify architecture, exports, dependency closure, executable bits, rpaths/install names, duplicate libraries, and missing shader formats.
- [x] Build deterministic archives using Zig tools and compare two clean manifest/hash runs. The frozen Windows x64 worktree at `566285eba` produced the identical full-package SHA-256 `73453868F3D57542789722314CAB11F2AB02D565EC9FF6944CA308F1A0E46446` on two runs; the package tool also passed its 102-entry ordering/readability/determinism fixture.
- [ ] Run `install-game`, `verify-runtime`, and `package-game` for all targets in CI/native eligibility rules.
- [ ] Commit: `build: close cross-platform package matrix`

**Evidence:** sorted package manifests, hashes, and verifier reports.

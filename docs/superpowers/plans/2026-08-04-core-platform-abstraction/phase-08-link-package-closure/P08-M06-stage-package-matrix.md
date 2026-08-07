# P08-M06 — Close the Stage and Package Matrix

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Produce deterministic runnable layouts for all three targets with complete runtime verification.

**Dependencies:** P08-M05.

**Allowed files:** `tools/zig/stage.zig`, `tools/zig/stage_test.zig`, `tools/zig/package.zig`, `tools/zig/verify_runtime.zig`, `build.zig`, `.gitignore`, `.github/workflows/cross-platform.yml`.

- [x] Define exact target manifests for Game, PlatformRuntime, modules, SDL3, renderer, shaders, Data/config roots, metadata, and licenses. `verify_runtime.zig` walks staged roots and rejects missing, duplicate, foreign, unsafe, cache/temp, and user-write entries; staging requires `Data/Configs/defconf.cfg`, `LICENSE.md`, and `README.md`.
- [x] Test staging into paths with spaces and non-ASCII characters and reject cache/temp/user-write files. `stage_test.zig` covers a copied fixture at a path containing spaces and Cyrillic characters; staging excludes forbidden artifact paths.
- [ ] Verify architecture, exports, dependency closure, executable bits, rpaths/install names, duplicate libraries, and missing shader formats.
- [x] Run the Windows x64 `install-game`, `verify-runtime`, and `package-game` graph; `verify-runtime` passes 10/10 and the game-only ZIP is emitted. All-target native/CI execution remains open.
- [x] Produce side-by-side Windows x64 Debug and ReleaseFast staged trees with `-Dbuild-variant=debug|release`; native runtime verification and the C6 main-menu startup smoke pass for both.
- [x] Build deterministic archives using Zig tools and compare two full Windows x64 runs at `a8bfef3e8`. Both metadata-aware archives were 3,073,345,577 bytes with identical SHA-256 `4264279A1FD6247B51FF2ECB9E5748E98C1E362A685C211E1C41CFB56135A802`; the package tool also passed its 102-entry ordering/readability/determinism fixture.
- [ ] Run `install-game`, `verify-runtime`, and `package-game` for all targets in CI/native eligibility rules.
- [ ] Commit: `build: close cross-platform package matrix`

**Evidence:** sorted package manifests, hashes, and verifier reports.

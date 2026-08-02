# P00-M05 — Establish the Foundation Compile Matrix

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Turn Phase 00 contracts into one repeatable matrix and record the remaining compile frontier.

**Dependencies:** P00-M02, P00-M04.

**Allowed files:** `build.zig`, `tools/zig/platform_build_matrix_test.zig`, `docs/superpowers/evidence/platform-port/build-hermeticity.md`, `docs/superpowers/evidence/platform-port/target-matrix.md`.

- [ ] Add `platform-foundation` and `test-platform-foundation` steps containing build-support, portable-header, stage, shader parser, and hermeticity tests.
- [ ] Make non-native test steps compile artifacts without trying to execute them.
- [ ] Record exact Zig/SDL/dependency revisions, target triples, native/cross status, sysroot requirements, and current first compile blocker per target.
- [ ] Run the Phase 00 exit commands from `MANIFEST.md` twice to prove deterministic output.
- [ ] Verify `git status --ignored --short` shows all generated outputs ignored and no source/evidence output under cache roots.
- [ ] Commit: `test: establish platform foundation matrix`

**Evidence:** committed matrix and hermeticity records; no runtime claim for cross-only targets.

# P09-M06 — Enforce CI, Documentation, and Final Release Matrix

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Make Windows/Linux/macOS support reproducible and close the milestone with all native evidence current.

**Dependencies:** P09-M05.

**Allowed files:** `.github/workflows/platform.yml`, `README.md`, `build.zig`, `docs/superpowers/evidence/platform-port/target-matrix.md`, `docs/superpowers/evidence/platform-port/windows-regression.md`, `docs/superpowers/evidence/platform-port/release-checklist.md`.

- [ ] Add native CI jobs for Windows x64, Linux x64, and macOS arm64: foundation/platform tests, target build, shader generation/determinism, package verifier, renderer smoke where runner GPU support is available, and game automatic smoke where display/audio constraints permit.
- [ ] Keep compile-only and native-run results labeled separately; do not mark skipped GPU/human gates successful.
- [ ] Document prerequisites, supported triples, build/install/run commands, data/user/log/save paths, package layouts, GPU drivers/shader formats, and known exclusions.
- [ ] From clean native hosts re-run final Windows acceptance automation, Linux accepted commands, and macOS accepted commands against one commit.
- [ ] Verify shell audit, ignored-cache status, package hashes, zero renderer counts, and signed human evidence for Linux/macOS.
- [ ] Commit: `ci: accept cross platform game runtime`

**Evidence:** CI run links/IDs in the release checklist, final three-platform matrix, and Windows regression record.

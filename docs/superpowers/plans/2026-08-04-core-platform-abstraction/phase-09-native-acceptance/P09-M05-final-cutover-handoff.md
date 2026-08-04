# P09-M05 — Final Cutover and Portability Handoff

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Remove temporary compatibility paths and declare PlatformRuntime the sole playable platform boundary.

**Dependencies:** P09-M02, P09-M03, P09-M04.

**Allowed files:** `Sources/src/Platform/PortableCrt.h`, `tools/zig/runtime_platform_allowlist.txt`, `tools/zig/runtime_platform_audit.zig`, `build.zig`, `README.md`, `docs/superpowers/plans/2026-08-02-linux-macos-platform-port/NEXT.md`, `docs/superpowers/plans/2026-08-04-core-platform-abstraction/README.md`, `docs/superpowers/plans/2026-08-04-core-platform-abstraction/NEXT.md`, `docs/superpowers/evidence/platform-abstraction/final-audit.md`, `docs/superpowers/evidence/platform-abstraction/linux-acceptance.md`, `docs/superpowers/evidence/platform-abstraction/macos-acceptance.md`, `docs/superpowers/evidence/platform-abstraction/windows-regression.md`.

- [ ] Remove obsolete host-service shims from `PortableCrt.h`, the temporary native allowlist, DirectInput oracle from default builds, and dead platform source paths.
- [ ] Run the full Windows/Linux/macOS workflow matrix, package verification, audit, platform ABI tests, module tests, renderer tests, and native acceptance checks.
- [ ] Verify public docs list target requirements, native test commands, writable data roots, package layouts, and known non-goals.
- [ ] Record final commit/artifact hashes and every human acceptance result.
- [ ] Mark the old Linux/macOS plan frontier superseded and this plan complete.
- [ ] Commit: `platform: complete cross-platform host abstraction`

**Evidence:** empty allowlist, full green matrix, final artifact hashes, and signed-off handoff.

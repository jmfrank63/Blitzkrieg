# P07-M06 — Enforce Zero Native Residue

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Empty the temporary allowlist for playable modules and make native dependency regressions fail the build.

**Dependencies:** P07-M05.

**Allowed files:** `tools/zig/runtime_platform_audit.zig`, `tools/zig/runtime_platform_allowlist.txt`, `tools/zig/runtime_platform_audit_test.zig`, `build.zig`, `docs/superpowers/evidence/platform-abstraction/final-audit.md`.

- [x] Expand fixtures to cover every native header, type, call, pragma-link, library, backslash include, and wrong-case include encountered during migration.
- [x] Restrict native tokens to explicit backend directories and the Windows resource adapter.
- [x] Require the playable-module allowlist to be empty; fail on stale allowlist lines.
- [x] Run the audit from Windows and Linux and compare sorted results.
- [x] Add the audit to `game-all`, CI, and package verification dependencies.
- [x] Commit: `test: enforce portable playable source boundary`

**Evidence:** `final-audit.md` records the Windows zero-hit run and the matching
Linux CI command. The local Windows checkout has no WSL distribution available;
Linux execution is therefore retained as a CI gate.

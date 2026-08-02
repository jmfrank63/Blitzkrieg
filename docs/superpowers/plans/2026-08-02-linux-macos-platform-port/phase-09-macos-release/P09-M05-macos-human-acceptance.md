# P09-M05 — Run and Record macOS Human Acceptance

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Obtain explicit human approval of the native Apple-Silicon game.

**Dependencies:** P09-M04.

**Allowed files:** `docs/superpowers/evidence/platform-port/macos-acceptance.md`, `docs/superpowers/evidence/platform-port/target-matrix.md`.

- [ ] Human executes the recorded bundle and completes every P09-M04 checklist item.
- [ ] Record each item pass/fail, concrete observations, hardware/session facts, and paths/hashes for local captures; do not commit captures/logs/saves/packages.
- [ ] If rejected, stop and create focused remediation packets before acceptance; do not modify production files here.
- [ ] After accepted, re-run native automatic smoke/endurance from a clean checkout and verify cited commits/package hashes.
- [ ] Update target matrix macOS status to native accepted.
- [ ] Commit: `test: accept native Apple Silicon game runtime`

**Evidence:** signed accepted macOS checklist and rerun transcript.

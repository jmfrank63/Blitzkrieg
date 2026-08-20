# P02-M03 — error classification and recovery

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Turn terse rc errors into named outcomes the UI can offer a real choice about.

**Dependencies:** P02-M02.

**Allowed files:** `Sources/src/CloudSync/engine.zig`, `Sources/src/CloudSync/engine_test.zig`.

- [ ] Write the failing test with captured real failure texts as fixtures, not invented ones.
- [ ] Implement `classify(failure: RcFailure, log: []const u8) Outcome` over `Outcome = enum { needs_resync, too_many_deletes, name_too_long, auth_failed, remote_unreachable, daemon_gone, unknown }`.
- [ ] Match `must run --resync to recover` to `.needs_resync`, `too many deletes` to `.too_many_deletes`, `file name too long` to `.name_too_long`. The rc reply itself says only `{"error": "bisync aborted", "status": 500}`, so classification must read the log, not the reply.
- [ ] Map each outcome to a recovery action: `.needs_resync` offers a confirmed re-pair; `.too_many_deletes` offers "the cloud copy looks emptied, mirror that?"; `.name_too_long` reports the projected length and points at the short link; `.auth_failed` opens the credentials dialog.
- [ ] `.too_many_deletes` must never auto-retry with `force`. That is the guard working, and overriding it silently is the one behaviour this design exists to prevent.
- [ ] Keep the last 200 lines of the failing run log addressable through the C ABI error string for support purposes.
- [ ] Commit checkpoint: `cloudsync: classify sync failures into recoverable outcomes`.

**Evidence:** Unit tests map each captured failure fixture to its outcome, including the bare `bisync aborted` reply whose cause is only in the log.

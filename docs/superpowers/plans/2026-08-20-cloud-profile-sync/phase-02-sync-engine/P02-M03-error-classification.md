# P02-M03 — error classification and recovery

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Turn terse rc errors into named outcomes the UI can offer a real choice about.

**Dependencies:** P02-M02.

**Allowed files:** `Sources/src/CloudSync/engine.zig`, `Sources/src/CloudSync/engine_test.zig`.

- [ ] Write the failing test with captured real failure texts as fixtures, not invented ones.
- [ ] Implement `classify(failure: RcFailure, log: []const u8) Outcome` over `Outcome = enum { needs_resync, too_many_deletes, name_too_long, out_of_sync, auth_failed, remote_unreachable, daemon_gone, timed_out, unknown }`.
- [ ] Match `must run --resync to recover` to `.needs_resync`, `too many deletes` to `.too_many_deletes`, `file name too long` to `.name_too_long`, `path1 and path2 are out of sync` to `.out_of_sync`. The rc reply says only `{"error": "bisync aborted", "status": 500}`, so classification must read the log, never the reply.
- [ ] Map each outcome to a recovery action: `.needs_resync` and `.out_of_sync` offer a confirmed re-pair; `.too_many_deletes` asks "the cloud copy looks emptied, mirror that?"; `.name_too_long` reports the projected length and points at the short link; `.auth_failed` opens the credentials dialog; `.timed_out` offers a retry.
- [ ] `.too_many_deletes` must never auto-retry with `force`. That is the guard working, and silently overriding it is the one behaviour this design exists to prevent.
- [ ] Keep the last 200 lines of the failing run log reachable through the ABI error string for support purposes, with credentials redacted.
- [ ] Commit checkpoint: `cloudsync: classify sync failures into recoverable outcomes`.

**Evidence:** Unit tests map each captured fixture to its outcome, including the bare `bisync aborted` reply whose cause exists only in the log.

# P03-M04 — connection test

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Tell the player what is wrong before they discover it at the next sync.

**Dependencies:** P03-M03.

**Allowed files:** `Sources/src/CloudSync/engine.zig`, `Sources/src/CloudSync/engine_test.zig`, `Sources/src/CloudSync/cloudsync.zig`, `Sources/src/CloudSync/CloudSync.def`, `Sources/src/CloudSync/CloudSync.x64.def`, `tools/zig/cloudsync_abi_test.cpp`.

- [ ] Write the failing test with fixtures for a wrong key, a wrong bucket, an unreachable endpoint, and a good configuration.
- [ ] Implement `testConnection(allocator, ctx) !TestResult` issuing `operations/list` against the configured remote root with a short deadline, executed on the worker like every other rc call.
- [ ] Distinguish authentication failure from an unreachable endpoint from a missing bucket. "It did not work" is the outcome this packet exists to eliminate.
- [ ] Report through the P02-M03 `Outcome` vocabulary so the dialog and the sync path speak with one voice.
- [ ] Export `bk_cloudsync_test_connection() i32` returning a handle polled like a sync, since it is a network call and must not block either. Own the whole export path.
- [ ] Never log or display the secret on any failure path; route everything through `Credentials.redacted()`.
- [ ] Commit checkpoint: `cloudsync: connection test with classified failures`.

**Evidence:** Each fixture maps to its distinct outcome, the call is pollable rather than blocking, and no secret appears in any output.

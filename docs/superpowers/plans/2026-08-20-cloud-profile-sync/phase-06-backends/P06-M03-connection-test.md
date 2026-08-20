# P06-M03 — connection test and failure reporting

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Tell the player what is wrong before they discover it at the next sync.

**Dependencies:** P06-M02.

**Allowed files:** `Sources/src/CloudSync/engine.zig`, `Sources/src/GameTT/InterfaceCloudCredentials.cpp`.

- [ ] Write the failing test with fixtures for a wrong key, a wrong bucket, an unreachable endpoint, and a good configuration.
- [ ] Implement `testConnection(allocator, client, ctx) !TestResult` issuing `operations/list` with a short timeout against the configured remote root.
- [ ] Distinguish authentication failure from an unreachable endpoint from a missing bucket. "It did not work" is the outcome this packet exists to eliminate.
- [ ] Report through the P02-M03 outcome vocabulary so the dialog and the sync path speak with one voice.
- [ ] Never log or display the secret in any failure path; route everything through `Credentials.redacted()`.
- [ ] Commit checkpoint: `cloudsync: connection test with classified failures`.

**Evidence:** Each fixture maps to its distinct message, with no secret present in any output.

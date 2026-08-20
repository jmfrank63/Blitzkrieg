# P02-M01 — pairing and resync bootstrap

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Establish the first pairing for a profile, and never silently repeat it.

**Dependencies:** P01-M04.

**Allowed files:** `Sources/src/CloudSync/engine.zig`, `Sources/src/CloudSync/engine_test.zig`.

- [ ] Write the failing integration test against a live daemon and two local directories: first run pairs, second run does not resync.
- [ ] Implement `PairingState` persisted at `profiles/<name>/.cloudsync-state` recording `paired: bool`, `last_success_unix: i64`, and the remote fingerprint used at pairing.
- [ ] Implement `Engine.pair(self, ctx) !void` issuing bisync with `resync: true`, writing the sentinel first via `plan.ensureSentinel`.
- [ ] Refuse to auto-resync a profile that is already paired. A resync after real divergence overwrites one side, so recovery is an explicit player action routed through P02-M03, never an automatic retry.
- [ ] Detect a changed remote fingerprint (different bucket or endpoint) and treat it as a new pairing requiring confirmation rather than as a resumable session.
- [ ] Commit checkpoint: `cloudsync: pair a profile with the remote`.

**Evidence:** Integration output shows a successful resync followed by a second run whose parameters contain no `resync` key.

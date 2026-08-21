# P03-M03 — token storage and refresh

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Keep the authorisation working after the first sync.

**Dependencies:** P03-M02.

**Allowed files:** `Sources/src/CloudSync/creds.zig`, `Sources/src/CloudSync/creds_test.zig`, `Sources/src/CloudSync/oauth.zig`, `Sources/src/CloudSync/engine.zig`, `Sources/src/CloudSync/worker.zig`, `Sources/src/CloudSync/worker_test.zig`, `Sources/src/CloudSync/daemon.zig`, `build.zig`.

The read-back points are where a job finishes and where the rc client is torn down — both inside `worker.zig` — so it is in scope alongside the engine and the daemon.

- [ ] Write the failing test with an expired token fixture.
- [ ] Store the token as an ordinary option value, since rclone treats it as one. It is `Sensitive`, so the existing withheld-secret contract covers it with no new mechanism.
- [ ] **Let rclone refresh it, then read it back — "persist what rclone writes" is not an implementation.** rclone writes the refreshed token into *its* config, not ours, so it must be read out explicitly with `config/get` for the remote (or `config/dump` when the whole set is wanted) and written to `cloud.credentials`.
- [ ] Define exactly when the read-back happens: immediately after the authorisation flow completes, and after any operation that may have refreshed the token — a completed sync, a connection test, a backup run. A token refreshed and never read back is lost when the daemon exits.
- [ ] Read back once more before the rc client is destroyed. That teardown happens in `worker.zig`, ahead of `Daemon.shutdown()`, so the last read-back must be sequenced there — after the client is gone there is nothing left to ask.
- [ ] **Cover that sequencing with a worker test**, not by inspection: a token refreshed during the final job must be on disk after teardown. `worker_test.zig` is in the allowlist, and `build.zig` too if a new target is needed.
- [ ] Write through the same atomic temp-then-rename path the credentials file already uses; a token half-written is an unusable credential.
- [ ] Detect a token that can no longer be refreshed and map it to the existing `auth_failed` outcome, which already routes the player to the credentials dialog.
- [ ] Confirm the token never reaches a log, an error string, or the config backup — `cloud.credentials` is excluded from both sync and backup, and a token living anywhere else would break that.
- [ ] Commit checkpoint: `cloudsync: persist and refresh OAuth tokens`.

**Evidence:** A token survives a restart and a sync; an unrefreshable token reports `auth_failed`; no token appears in logs, error strings or backups.

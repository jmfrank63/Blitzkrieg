# P03-M03 — token storage and refresh

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Keep the authorisation working after the first sync.

**Dependencies:** P03-M02.

**Allowed files:** `Sources/src/CloudSync/creds.zig`, `Sources/src/CloudSync/creds_test.zig`, `Sources/src/CloudSync/oauth.zig`.

- [ ] Write the failing test with an expired token fixture.
- [ ] Store the token as an ordinary option value, since rclone treats it as one. It is `Sensitive`, so the existing withheld-secret contract covers it with no new mechanism.
- [ ] **Let rclone refresh it.** rclone owns the refresh cycle when it holds the token; our job is to persist what it writes back, not to implement OAuth refresh ourselves.
- [ ] Detect a token that can no longer be refreshed and map it to the existing `auth_failed` outcome, which already routes the player to the credentials dialog.
- [ ] Confirm the token never reaches a log, an error string, or the config backup — `cloud.credentials` is excluded from both sync and backup, and a token living anywhere else would break that.
- [ ] Commit checkpoint: `cloudsync: persist and refresh OAuth tokens`.

**Evidence:** A token survives a restart and a sync; an unrefreshable token reports `auth_failed`; no token appears in logs, error strings or backups.

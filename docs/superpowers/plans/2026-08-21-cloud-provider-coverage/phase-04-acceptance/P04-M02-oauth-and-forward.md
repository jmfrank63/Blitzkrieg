# P04-M02 — OAuth and forward compatibility

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Prove the consumer clouds work, and that a newer rclone needs no game change.

**Dependencies:** P04-M01.

**Allowed files:** `docs/superpowers/evidence/cloud-sync/p04-m02-oauth-forward.md`.

- [ ] Configure one OAuth backend — Drive, Dropbox or OneDrive — from consent to a completed sync, and again after a restart to prove the token persisted.
- [ ] **Test the forward-compatibility claim directly.** Stage a newer rclone than the one the catalogue was captured from, and confirm a backend absent from the old catalogue appears in the provider list and can be configured, with no game rebuild. That claim is the reason this plan exists; it must be demonstrated, not argued.
- [ ] Confirm the cache refreshes when the rclone version changes, rather than serving a stale catalogue.
- [ ] Confirm a backend removed upstream does not break a profile already configured with it.
- [ ] Human approval required.
- [ ] Commit checkpoint: `cloudsync: OAuth and forward-compatibility acceptance`.

**Evidence:** Evidence shows an OAuth backend syncing across a restart, and a provider new to a newer rclone configured without rebuilding the game.

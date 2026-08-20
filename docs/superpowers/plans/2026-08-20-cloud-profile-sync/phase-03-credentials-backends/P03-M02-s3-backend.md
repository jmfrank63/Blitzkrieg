# P03-M02 — S3-compatible backend

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Reach S3-compatible storage with nothing but stored credentials.

**Dependencies:** P03-M01.

**Allowed files:** `Sources/src/CloudSync/creds.zig`, `Sources/src/CloudSync/backend_test.zig`.

- [ ] Write the failing integration test against a local MinIO instance, so the gate depends on neither a paid account nor a network.
- [ ] Emit `{"type": "s3", "provider": ..., "access_key_id": ..., "secret_access_key": ..., "endpoint": ..., "region": ...}` from `remoteParams`. rclone signs the requests; this plan implements no SigV4.
- [ ] Set `_name` so the remote contributes a short `name:root` to the session name rather than a long URL.
- [ ] Run the whole phase-02 cycle against the remote: pair with `resyncMode`, diverge on both sides, converge, conflict preserved as `.conflictN`, delete propagated and recoverable from **both** trashes.
- [ ] This is the first packet where `backupDir2` is genuinely remote. Confirm it resolves on the remote filesystem and sits outside the synced prefix; a local path here fails with `parameter to --backup-dir has to be on the same remote as destination`.
- [ ] Record which providers the test covers and which are inferred. MinIO passing does not prove Cloudflare R2 or Backblaze B2 — say so rather than implying coverage.
- [ ] Commit checkpoint: `cloudsync: S3-compatible backend`.

**Evidence:** Integration run against MinIO shows the full cycle passing, the remote trash populated, and the session-name length recorded.

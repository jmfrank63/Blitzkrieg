# P06-M01 — S3-compatible backend

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Reach S3-compatible storage with nothing but stored credentials.

**Dependencies:** P04-M04 and P05-M04.

**Allowed files:** `Sources/src/CloudSync/creds.zig`, `Sources/src/CloudSync/backend_test.zig`.

- [ ] Write the failing integration test against a local MinIO instance, so the gate does not depend on a paid account or a network.
- [ ] Emit the inline remote object from `Credentials.remoteParams`: `{"type": "s3", "provider": ..., "access_key_id": ..., "secret_access_key": ..., "endpoint": ..., "region": ...}`. rclone signs the requests; this plan implements no SigV4.
- [ ] Set `_name` so the remote contributes a short `name:root` to the bisync session name rather than a long path — `bilib.FsPath` charges full length only to the `local` branch.
- [ ] Verify the whole phase-02 cycle against the remote: pair, diverge on both sides, converge, conflict preserved as `.conflictN`, delete propagated and recoverable from the trash.
- [ ] Record which providers the test covers and which are inferred. MinIO passing does not prove Cloudflare R2 or Backblaze B2; say so rather than implying coverage.
- [ ] Commit checkpoint: `cloudsync: S3-compatible backend`.

**Evidence:** Integration run against MinIO shows the full cycle passing, with the session name length recorded.

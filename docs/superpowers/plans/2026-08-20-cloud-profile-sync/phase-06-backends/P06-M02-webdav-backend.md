# P06-M02 — WebDAV backend

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Reach self-hosted storage over WebDAV.

**Dependencies:** P06-M01.

**Allowed files:** `Sources/src/CloudSync/creds.zig`, `Sources/src/CloudSync/backend_test.zig`.

- [ ] Write the failing integration test against a local WebDAV server.
- [ ] Emit `{"type": "webdav", "url": ..., "vendor": ..., "user": ..., "pass": ...}`; rclone obscures the password itself, so store the plain value and let it handle the transform.
- [ ] Run the same phase-02 cycle unchanged. A backend that needs the cycle altered is not finished.
- [ ] Check modification-time fidelity explicitly: `conflictResolve: "newer"` rests on timestamps, and some WebDAV servers round or drop them. If the vendor cannot hold mtimes, report it rather than shipping silent conflict misresolution.
- [ ] Commit checkpoint: `cloudsync: WebDAV backend`.

**Evidence:** Integration run shows the full cycle passing, with observed modification-time granularity recorded.

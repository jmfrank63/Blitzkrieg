# P03-M03 — WebDAV backend

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Reach self-hosted storage over WebDAV.

**Dependencies:** P03-M02.

**Allowed files:** `Sources/src/CloudSync/creds.zig`, `Sources/src/CloudSync/backend_test.zig`.

- [ ] Write the failing integration test against a local WebDAV server.
- [ ] Emit `{"type": "webdav", "url": ..., "vendor": ..., "user": ..., "pass": ...}` from the `webdav` arm of the P03-M01 tagged union; rclone obscures the password itself, so store the plain value and let it transform.
- [ ] Run the same phase-02 cycle unchanged. A backend that needs the cycle altered is not finished.
- [ ] Check modification-time fidelity explicitly: both `conflictResolve: "newer"` and `resyncMode: "newer"` rest on timestamps, and some WebDAV servers round or drop them. A vendor that cannot hold mtimes must be reported, not shipped with silent conflict misresolution.
- [ ] Commit checkpoint: `cloudsync: WebDAV backend`.

**Evidence:** Integration run shows the full cycle passing, with observed modification-time granularity recorded.

# P04-M01 — static-credential backends end to end

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Show the generic path works on services nobody wrote code for.

**Dependencies:** P03-M03.

**Allowed files:** `docs/superpowers/evidence/cloud-sync/p04-m01-backends.md`.

- [ ] Configure and sync three backends chosen for their differences, entirely through the generic form: one S3-compatible, one WebDAV, and one that is neither — SFTP or Backblaze B2 native.
- [ ] Run the full phase-02 cycle from the sync plan against each: pair, diverge on both sides, converge, conflict preserved, delete recoverable from both trashes.
- [ ] Confirm the bisync session name stays inside the 241-byte budget for each, since a new backend brings a new remote-name shape.
- [ ] Record which services were tested and which are inferred. **Three passing proves three.** There is no supported-provider count to quote: eleven of rclone's 69 backends are filtered as non-candidates, and the remainder are candidates whose writable behaviour is established per configuration by the connection test, not by membership in a list. State the tested set and say the rest are untested.
- [ ] Human approval required.
- [ ] Commit checkpoint: `cloudsync: static-credential backend acceptance`.

**Evidence:** Evidence records each backend with commands, session-name lengths, trash contents after the delete, and human approval.

# P02-M04 — validation and connection test

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Make required fields and the connection test work for a backend nobody wrote code for.

**Dependencies:** P02-M03.

**Allowed files:** `Sources/src/CloudSync/form.zig`, `Sources/src/CloudSync/form_test.zig`, `Sources/src/CloudSync/engine.zig`, `Sources/src/GameTT/InterfaceCloudCredentials.cpp`.

- [ ] Write the failing test with a fixture backend whose required field is blank.
- [ ] Validate `Required` fields from the catalogue before the connection test runs, naming the field by its catalogue `Help` rather than a generic message.
- [ ] Reuse `testConnection` unchanged — it issues `operations/list` against whatever remote parameters it is given and is already backend-agnostic. If it needs changing for a new backend, something upstream of it is hardcoded.
- [ ] Confirm the remote root participates: a bucket typo must fail the test, not appear to succeed against the account root.
- [ ] **Test writability, not just listing.** A successful `operations/list` proves the credentials resolve; it does not prove the backend accepts writes and deletes, which bisync requires — some are read-only or restrict deletion. Write a small probe object under the remote root, read it back, and delete it, reporting a listable-but-not-writable remote as its own outcome rather than passing it through to fail at the first sync.
- [ ] Clean the probe up on every path, including failure. A stray probe object in a player's bucket is our litter.
- [ ] Map failures through the existing `Outcome` vocabulary so the dialog and the sync path keep one voice.
- [ ] Keep every secret out of logs and error strings through `redacted()`; a generic form makes it easy to leak a field nobody anticipated.
- [ ] Commit checkpoint: `cloudsync: validate and test an arbitrary backend`.

**Evidence:** A backend configured entirely through the generic form connects; a missing required field is named before any network call; a wrong remote root fails rather than silently succeeding.

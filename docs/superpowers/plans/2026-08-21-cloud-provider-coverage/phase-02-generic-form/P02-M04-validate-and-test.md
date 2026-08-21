# P02-M04 — validation and connection test

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Make required fields and the connection test work for a backend nobody wrote code for.

**Dependencies:** P02-M03.

**Allowed files:** `Sources/src/CloudSync/form.zig`, `Sources/src/CloudSync/form_test.zig`, `Sources/src/CloudSync/engine.zig`, `Sources/src/CloudSync/engine_test.zig`, `Sources/src/GameTT/InterfaceCloudCredentials.cpp`, `Data/Textes/UI/CloudCredentials`, `Sources/src/Main/CloudSyncFacade.h`, `Sources/src/Main/CloudSyncFacade.cpp`.

- [ ] Write the failing test with a fixture backend whose required field is blank.
- [ ] Validate `Required` fields from the catalogue before the connection test runs, naming the field by its catalogue `Help` rather than a generic message.
- [ ] **Extend `testConnection`** — an earlier draft said to reuse it unchanged *and* to make it write, which cannot both hold. It stays backend-agnostic: if it ever needs a per-backend branch, something upstream is hardcoded and that is a stop condition. But the operation itself grows from listing to a write probe.
- [ ] Confirm the remote root participates: a bucket typo must fail the test, not appear to succeed against the account root.
- [ ] **Test writability, not just listing.** A successful `operations/list` proves the credentials resolve; it does not prove the backend accepts writes and deletes, which bisync requires — some are read-only or restrict deletion. Write a small probe object under the remote root, read it back, then delete it.
- [ ] **Upload the probe with `operations/copyfile` from a local temporary file, not `operations/uploadfile`.** `rc.Client.call` posts a JSON body and nothing else, while `uploadfile` needs multipart — reaching for it would mean growing a second transport in the client for the sake of a probe. Write the probe locally, copy it up, remove the local copy.
- [ ] Add a typed outcome for the listable-but-not-writable case. The current `Outcome` enum has no such member, and mapping it onto an existing one would tell the player the wrong thing — hence `engine.zig` and the UI text resource are both in the allowlist.
- [ ] **Accept that cleanup can fail, because that is the condition under test.** If the upload succeeds and the delete is refused, the probe cannot be removed — demanding cleanup "on every path" is impossible exactly when it matters. Name the probe unmistakably (a fixed prefix plus a nonce, under the configured root), delete it on every path where deletion is permitted, and when it is not, **report the exact path left behind** so the player can remove it themselves.
- [ ] Say so in the outcome text as well: a remote that accepts writes but refuses deletes cannot support sync, and it has one of our probe files in it.
- [ ] Map failures through the existing `Outcome` vocabulary so the dialog and the sync path keep one voice.
- [ ] Keep every secret out of logs and error strings through `redacted()`; a generic form makes it easy to leak a field nobody anticipated.
- [ ] Commit checkpoint: `cloudsync: validate and test an arbitrary backend`.

**Evidence:** A backend configured entirely through the generic form connects; a missing required field is named before any network call; a wrong remote root fails rather than silently succeeding.

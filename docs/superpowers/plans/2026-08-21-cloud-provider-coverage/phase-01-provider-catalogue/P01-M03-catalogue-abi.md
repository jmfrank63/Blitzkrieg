# P01-M03 — catalogue and credentials exports

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Make the catalogue and the new schema reachable from C++, since nothing above Zig can see either yet.

**Dependencies:** P01-M02.

**Allowed files:** `Sources/src/CloudSync/catalogue.zig`, `Sources/src/CloudSync/creds.zig`, `Sources/src/CloudSync/cloudsync.zig`, `Sources/src/CloudSync/CloudSync.def`, `Sources/src/CloudSync/CloudSync.x64.def`, `tools/zig/cloudsync_abi_test.cpp`, `Sources/src/Main/CloudSyncFacade.h`, `Sources/src/Main/CloudSyncFacade.cpp`.

- [ ] Extend the C++ ABI smoke test first to enumerate providers and round-trip a credentials document, and watch it fail.
- [ ] Add the exports and their facade wrappers in this one commit, per the ABI amendment rule: provider enumeration, per-provider option enumeration, and credentials load/save against the generic schema.
- [ ] **This packet exists because the previous draft had none.** The catalogue lived in Zig and the dialog lived in C++ with no packet owning the chain between them, so the form could never have been rendered. Any later packet that needs a new export owns its chain the same way.
- [ ] Serialise across the boundary as JSON into a caller-supplied buffer, matching `discovery_status`, rather than inventing a second marshalling style.
- [ ] Report a too-small buffer as a required size rather than truncating, since option sets vary by orders of magnitude between backends.
- [ ] Keep the withheld-secret contract at the boundary: the load export returns per-field `has_value` flags, never secret values.
- [ ] **Replace `bk_cloudsync_creds_clear_secret()`, which takes no arguments.** It was written when a backend had one secret; the generic schema can carry several — an S3 secret key, an SFTP passphrase, an OAuth token — and an argument-free clear cannot say which. Provide a per-field clear naming the option, and keep the old export only if something still calls it, in which case document what it clears.
- [ ] Decide and document the save representation of "clear this field": an explicitly empty value must be distinguishable from an omitted one, because omitted means *preserve* under the existing contract. Getting this wrong silently keeps a credential the player asked to remove.
- [ ] Export `ensureCatalogue` from P01-M01 as part of this chain, since the dialog in P02-M03 needs to trigger it and cannot reach Zig directly. **Export it as a pollable job — begin, poll, cancel, release — not as a blocking call**, matching every other export that can touch a socket. A synchronous catalogue fetch would stall the UI thread through a daemon spawn.
- [ ] Commit checkpoint: `cloudsync: expose the catalogue and credentials through the ABI`.

**Evidence:** The C++ consumer enumerates providers, reads one backend's option list, round-trips a credentials document with secrets withheld, clears one named secret while leaving another intact, and triggers `ensureCatalogue`.

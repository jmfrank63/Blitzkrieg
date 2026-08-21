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
- [ ] Commit checkpoint: `cloudsync: expose the catalogue and credentials through the ABI`.

**Evidence:** The C++ consumer enumerates providers from the cached catalogue, reads one backend's option list, and round-trips a credentials document with secrets withheld.

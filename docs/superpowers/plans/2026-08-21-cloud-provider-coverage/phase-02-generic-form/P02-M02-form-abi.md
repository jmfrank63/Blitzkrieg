# P02-M02 — form exports

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Carry the form model across the ABI, because the renderer is C++ and the model is Zig.

**Dependencies:** P02-M01.

**Allowed files:** `Sources/src/CloudSync/form.zig`, `Sources/src/CloudSync/cloudsync.zig`, `Sources/src/CloudSync/CloudSync.def`, `Sources/src/CloudSync/CloudSync.x64.def`, `tools/zig/cloudsync_abi_test.cpp`, `Sources/src/Main/CloudSyncFacade.h`, `Sources/src/Main/CloudSyncFacade.cpp`.

- [ ] Extend the C++ ABI smoke test first to build a form for a named backend and read back its fields, and watch it fail.
- [ ] Add the exports and facade wrappers in one commit per the ABI amendment rule: build a form for a backend, enumerate its fields, and read each field's kind, label, help, examples, flags and placeholder.
- [ ] Serialise as JSON into a caller-supplied buffer like the other exports, reporting a required size rather than truncating — s3 and webdav differ by a factor of five in field count, so a fixed buffer is a bug waiting for a backend nobody tested.
- [ ] Keep the masked-field marking intact across the boundary; the renderer must not have to re-derive which fields are secret.
- [ ] Commit checkpoint: `cloudsync: expose the form model through the ABI`.

**Evidence:** The C++ consumer builds forms for four backends across the ABI and reads their fields, with a too-small buffer reporting a size rather than truncating.

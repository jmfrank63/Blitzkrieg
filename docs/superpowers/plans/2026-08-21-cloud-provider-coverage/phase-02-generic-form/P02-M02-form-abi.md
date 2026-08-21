# P02-M02 — form exports

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Carry the form model across the ABI, because the renderer is C++ and the model is Zig.

**Dependencies:** P02-M01.

**Allowed files:** `Sources/src/CloudSync/form.zig`, `Sources/src/CloudSync/cloudsync.zig`, `Sources/src/CloudSync/CloudSync.def`, `Sources/src/CloudSync/CloudSync.x64.def`, `tools/zig/cloudsync_abi_test.cpp`, `Sources/src/Main/CloudSyncFacade.h`, `Sources/src/Main/CloudSyncFacade.cpp`.

- [ ] Extend the C++ ABI smoke test first to build a form for a named backend and read back its fields, and watch it fail.
- [ ] Add the exports and facade wrappers in one commit per the ABI amendment rule: build a form for a backend, enumerate its fields, and read each field's kind, label, help, examples, flags and placeholder.
- [ ] **Carry the selected provider across the boundary**, not just the backend name. A build-by-backend-name export cannot express provider filtering, so it would stop at the Zig side and the dialog would render an unfiltered form.
- [ ] **Pass the backend and the selected provider only — not the current option map.** The form model needs neither to compute the filtered field list, and shipping the whole map would serialise freshly typed secret values across the boundary on every rebuild, which is a needless copy of exactly the data the withheld-secret contract exists to keep still. Preserving already-typed values is the dialog's job, matched by field name in C++.
- [ ] If a later packet finds the model genuinely needs the map, change the P02-M01 contract explicitly and exclude secret-classified fields from what crosses; do not widen this export quietly.
- [ ] Cover the rebuild in the ABI test: build for `s3` with provider `AWS`, then with `Wasabi`, and assert the `region` examples differ. That is the same assertion P02-M01 makes in Zig, repeated across the boundary, because a boundary that drops the argument passes every Zig test and still renders the wrong form.
- [ ] Serialise as JSON into a caller-supplied buffer like the other exports, reporting a required size rather than truncating — s3 and webdav differ by a factor of five in field count, so a fixed buffer is a bug waiting for a backend nobody tested.
- [ ] Keep the masked-field marking intact across the boundary; the renderer must not have to re-derive which fields are secret.
- [ ] Commit checkpoint: `cloudsync: expose the form model through the ABI`.

**Evidence:** The C++ consumer builds forms for four backends across the ABI and reads their fields; a rebuild for the same backend under two S3 vendors returns different examples; a too-small buffer reports a size rather than truncating.

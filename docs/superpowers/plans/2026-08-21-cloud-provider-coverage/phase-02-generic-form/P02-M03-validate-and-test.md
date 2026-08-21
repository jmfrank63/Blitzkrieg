# P02-M03 — validation and connection test

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Make required fields and the connection test work for a backend nobody wrote code for.

**Dependencies:** P02-M02.

**Allowed files:** `Sources/src/CloudSync/form.zig`, `Sources/src/CloudSync/engine.zig`, `Sources/src/GameTT/InterfaceCloudCredentials.cpp`.

- [ ] Write the failing test with a fixture backend whose required field is left blank.
- [ ] Validate `Required` fields from the catalogue before the connection test runs, naming the field with its catalogue `Help` rather than a generic message.
- [ ] Reuse the existing `testConnection` unchanged — it issues `operations/list` against whatever remote parameters it is given, so it is already backend-agnostic. If it needs a change to work with a new backend, something upstream of it is hardcoded.
- [ ] Map failures through the existing `Outcome` vocabulary so the dialog and the sync path keep speaking with one voice.
- [ ] Keep every secret out of logs and error strings via the existing `redacted()` path; a generic form makes it easier to leak a field nobody anticipated.
- [ ] Commit checkpoint: `cloudsync: validate and test an arbitrary backend`.

**Evidence:** A backend configured entirely through the generic form connects successfully, and a missing required field is reported by name before any network call.

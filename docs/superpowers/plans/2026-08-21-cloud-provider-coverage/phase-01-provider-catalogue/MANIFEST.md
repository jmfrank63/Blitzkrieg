# Phase 01 — Provider Catalogue

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Take the provider list from rclone, store it in a schema that can hold any backend, and make it reachable from C++.

| Packet | Depends on | Owns |
|---|---|---|
| P01-M01 | P00-M03 | catalogue fetch, parse, cache and bootstrap |
| P01-M02 | M01 | generic schema with remote root, migration, secret classification |
| P01-M03 | M02 | catalogue and credentials exports |
| P01-M04 | M03 | provider selection and destination filtering |

Exit: the provider list comes from the catalogue, reaches C++, survives a cold start, and old credentials still sync.

P01-M01 macOS checkpoint: `test-cloudsync-catalogue` 15/15, with worker 8/8 and
engine 16/16 covering the two files it amended; daemon, rc, abi, plan and
streamio unaffected; `x86_64-linux-gnu` compiles; no rclone process survives.
Commit `9aa47f7fb`.

Fixture: `tools/zig/fixtures/config_providers.json`, 264,837 bytes, captured
from a locally started `rclone rcd` on v1.75.0 and trimmed from the full
775,515-byte 69-backend reply to `s3`, `webdav`, `sftp`, `drive` and `dropbox`,
each kept **whole**. Provenance travels inside the file as a `_fixture` key,
which doubles as a genuine unknown top-level field the parser must tolerate.
Confirmed from the capture: exactly 18 option keys, no `Groups` anywhere,
`Provider` on 35 options and 664 examples across all 69 backends.

`ensureCatalogue` is a job, not a call. It reads only the cached document's
version stamp — a local file read — and returns `.cached` without enqueueing
when that matches, asserted by the worker staying `.idle`. A miss or version
change enqueues `JobKind.fetch_catalogue` and returns `.fetching` inside one
60 Hz frame. `Outcome.catalogue_ready` was appended, never reordered.

Two design choices worth carrying:

- The catalogue arm deliberately skips the short link **and** `applyCredentials`.
  The catalogue describes the binary, not a remote, so a broken credential must
  not be able to hide the very list a player needs to fix it.
- `refreshCache` asks `core/version` first and fetches `config/providers` only
  on a stamp mismatch, asserted by request line in the stub test. A
  stamped-but-empty document counts as a miss, so a zero-backend cache always
  refetches.

`matchProvider`, the `Hide` bitmask constants and `hiddenFromConfigurator` live
in `catalogue.zig` as their single owner. No provider, field or vendor name
appears in any source file.


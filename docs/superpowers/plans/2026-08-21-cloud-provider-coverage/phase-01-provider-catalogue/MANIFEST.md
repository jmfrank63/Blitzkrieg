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

P01-M02 macOS checkpoint: `test-cloudsync-creds` 15/15 against the generic
schema, migration asserted on byte-exact documents captured by running the
two-arm serializer before the change; rc 6, daemon 27, abi 10 + C++ consumer,
plan 35, catalogue 15, worker 8, engine 16, backup 21, facade, package 4,
verify-runtime 11, streamio 32 all green, the daemon/abi/worker/engine runs
repeated against a live staged rclone; backend 2/2 with the WebDAV cycle
executing end to end against `rclone serve webdav` on the generic schema; the
creds suite compiles for `x86_64-linux-gnu` and `x86_64-windows`. Commit
`06601b7c6`.

Findings the packet text does not carry:

- **The allowlist was widened to `backend_test.zig` with the plan owner's
  approval** (recorded in the packet): its two fixtures constructed the
  removed union, and no other packet owns the file.
- **The wire format keeps non-secret values flat** (`"endpoint":"…"`) with the
  classification as `secret_options`/`password_options` name arrays, because
  `cloudsync_abi_test.cpp` asserts the flat substring — per-entry option
  objects would have failed the untouched C++ consumer. The redacted form
  names stored secrets in `secret_options`; a dialog echoes those names and
  `mergeOmittedSecret` fills the values, which is also where the stored
  fingerprint is carried or released for re-derivation.
- **v1.75.0 flags more than the old dialog withheld**: s3 `access_key_id` and
  webdav `user` are `Sensitive`, so both are now flagged secret at migration.
  Only webdav `pass` is `IsPassword` among the legacy fields.
- **`parse` now dupes the document into its arena before JSON parsing.** The
  std parser references the input buffer for strings that need no unescaping;
  the two-arm `load` freed that buffer while the credentials still pointed
  into it — a latent use-after-free, fixed in passing (catalogue.zig had
  already documented the same hazard).
- **The shipped C++ dialog cannot prefill from the generic redacted document**
  until P01-M03/P02-M03 rewrite that chain; its legacy-format saves still
  parse via the migration path, and the ABI creds contract passes untouched.

P01-M02 amendment (2026-08-22), after review found two integration
regressions:

- **The migrated fingerprint now uses the facade scraper's format** —
  `{endpoint}/{bucket}` for S3, `{url}` for WebDAV, with the scraper's exact
  join semantics replicated. Review established that production pairing
  records hold the *facade's* scraped string, not this module's old
  `s3:`/`webdav:`-prefixed one, which nothing shipping ever consumed. The
  packet's "compute the legacy value exactly as the old code did" targeted
  the wrong old code; corrected in the packet. P01-M03 gains bullets to
  export the persisted fingerprint, switch the facade off its scraper (which
  degrades against the generic schema: S3 loses the bucket component, other
  backends scan to empty), and assert the continuity in the ABI consumer.
- **An interim guard in `InterfaceCloudCredentials`** refuses a save when the
  loaded document carries no `protocol` key (or is present but unreadable):
  the legacy dialog's prefill from a generic document is half-blank — for
  S3, vendor, bucket and access key scan to nothing — and an accept wrote
  the blanks back, blanking the remote root and rerouting the sync to the
  account root. The guard also covers the connection test, which saves
  before probing. It comes out when P01-M03/P02-M03 replace the dialog's
  document handling. WebDAV survives a blind reopen-and-save losslessly
  (url and vendor scan flat; user and pass merge), so the guard's cost is
  S3-only edits waiting one packet.
- **Transitional generic documents self-repair on load** (`dcb93e181`,
  follow-up review). Files written inside the `06601b7c6` window carry the
  old `s3:`/`webdav:`-prefixed fingerprint, and a stored fingerprint is
  never recomputed — so they would have demanded a re-pair forever once the
  facade consumes the persisted value. `parse` rewrites the fingerprint
  only when it is byte-equal to the old derivation of the same document's
  own components; a value merely resembling the old shape is an identity
  and stays verbatim, asserted by test.
- Suites re-run green with a live rclone, including the backend WebDAV
  cycle; `install-game` compiles the guarded dialog. Commits below.

P01-M03 macOS checkpoint: the extended C++ consumer failed first on exactly
the five missing exports, then abi 10/10 + consumer green natively and with
a live rclone (real fetch job: ensure → handle → done → `catalogue_ready`,
then rclone's own 69-backend list enumerates); facade suite green; the full
sweep (creds 17, rc 6, daemon 27, plan 35, catalogue 15, worker 8, engine
16, backup 21, backend 2 live, package 4, streamio 32) green;
`x86_64-linux-gnu` and `x86_64-windows` compile (the discovery version pin
in the consumer is POSIX-only — a script stub is not a Windows executable —
so the ensure-cached assertion is guarded there and the local reads run
everywhere). `install-game` compiles the new facade. Commit `41e0f64af`.

Findings the packet text does not carry:

- **The review's remaining P1 closes here**: `Fingerprint()` in the facade
  now reads the persisted value through `bk_cloudsync_creds_fingerprint`;
  the scraper is deleted, and the consumer asserts a migrated legacy
  document fingerprints as the byte-identical string the scraper produced.
- **Ensure lives at the export, not the worker.** `worker.ensureCatalogue`
  wants a live `Worker`, but the cached answer must not create one — the
  export reimplements the same stamp comparison from `catalogue.cachedVersion`
  and `matchesVersion`, and only a miss builds the worker and enqueues
  `fetch_catalogue`. The compared version is discovery's, so the cache
  always describes the binary that will serve the forms.
- **The vendor option is `provider` by rclone's own convention** (like
  `type`): every `Provider` expression references that option's value, so
  the cleanup naming it is structural, not backend knowledge.
- **The dialog guard stays** — this packet did not replace the dialog's
  document handling; P02-M03 owns both the rewrite and the guard's removal.
- The old no-arg `bk_cloudsync_creds_clear_secret` survives for its one
  caller (the legacy dialog), documented as clearing every withheld field;
  `bk_cloudsync_creds_clear_option` is the per-field act. Clearing through
  a save stays impossible by design: empty or omitted withheld entries
  always preserve.
- **Follow-up (`fcc9b9ebd`, review):** the facade's `BeginJob` kept a fixed
  1 KiB fingerprint buffer and emptied anything larger — and a pairing
  recorded against `""` can never detect a remote change. The fingerprint,
  its JSON escape, and the job document are now heap-sized under the
  required-size contract; the facade is otherwise still fixed-buffer
  no-STL, with this documented as the one exception. The facade test
  drives a 1.6 KiB identity across the ABI both ways, and its fixture
  cleans up after itself because the present-mode run reuses its directory
  across cached re-runs.


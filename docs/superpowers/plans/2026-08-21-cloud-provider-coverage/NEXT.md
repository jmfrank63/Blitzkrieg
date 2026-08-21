# Next Packet

Start at `phase-00-bundled-rclone/P00-M01-bundled-dependency.md`.

Nothing in this plan is implemented. The design at
`docs/superpowers/specs/2026-08-21-cloud-provider-coverage-design.md` was
measured against rclone v1.75.0: `config/providers` returns 69 backends with
fully self-describing options, S3 alone carries 53 vendor examples, and
`config/oauthstatus`/`config/oauthstop` exist alongside `config/create`'s
interactive state machine.

## What this plan changes in the shipped code

Two things, both from the cloud-profile-sync plan:

- `Sources/src/CloudSync/creds.zig` — `Protocol = enum { s3, webdav }` and its
  `Payload` union are replaced by a generic `{ backend, options }` map. This is
  a **revision of working, committed code**, so the migration path for already
  saved credentials is part of the packet, not an afterthought.
- `Data/Configs/defconf.cfg` — `Cloud.Provider` stops being a static droplist
  and is filled from the catalogue through `szActionFill`.

Everything else in that plan stands.

## Corrections applied after review

The first draft would not have executed. Seven issues, all confirmed against
the shipped code before being fixed:

- **Backend names would have been truncated.** `Cloud.Provider` was to carry
  the backend name, but `COptionSystem::Set` truncates strings over 12
  characters to 8 — `googlecloudstorage`, `internetarchive` and
  `oracleobjectstorage` would have become `googlecl`, `internet`, `oracleob`.
  The option is now `OFF`/`ON` and the identity lives in `cloud.credentials`.
- **The migration would have broken S3 bucket routing.** `remoteParams` says
  the bucket is "deliberately not here — for S3 it is a path component,
  carried by the alias target", and a `{backend, options}` schema had nowhere
  to put it. The schema is now `{backend, options, remote_root}`, with the
  fingerprint and alias target rebuilt from it.
- **No packet owned the Zig-to-C++ chain** for the catalogue or the form, so
  the renderer could never have received a form. P01-M03 and P02-M02 now own
  those export chains — the same gap the sync plan hit and fixed, repeated.
- **OAuth persistence was a sentence, not a design.** Refreshed tokens live in
  rclone's config; the read-back points (`config/get` after authorisation,
  after any refreshing operation, and before daemon shutdown) are now named.
- **A fresh install could sit at `OFF` forever** — the settings list may not
  start a daemon and an empty cache offers nothing, so nothing would ever
  populate it. P01-M01 now owns a background bootstrap fetch.
- **"Every backend is a provider" was too broad.** Twelve of the 69 are
  wrappers or non-destinations. P01-M04 filters them, and the acceptance
  packets no longer imply that four backends prove the rest. (The count and
  the membership were both corrected again in the next round.)
- **Storage assumptions were outgrown.** The 16 KiB caps in `creds.zig` were
  sized for "endpoints and keys"; the read path's failure mode is a silent
  null. Sizing is dynamic under a documented limit, and the secret flag is
  persisted per field so credentials load with no catalogue cached.

Two smaller ones: `build.zig.zon` is a static literal and cannot interpolate a
version into several URLs, and the "no provider names" invariant now carries
three declared exceptions — the legacy migration, the destination filter, and
the test fixture.

## Second correction round

Five more, all confirmed against the shipped code or against rclone itself:

- **The bootstrap packet could not implement its own requirement.** It called
  for a startup fetch while owning neither `GameMain.cpp` nor the export
  chain — and startup was the wrong place regardless, since `Available()` is
  reached only when cloud sync is already enabled, so a fresh profile would
  never have triggered it. Replaced by `ensureCatalogue()` on first need,
  triggered by the credentials dialog and refreshed after a successful sync.
- **OAuth owned none of its own path.** The state machine was to run on the
  worker without `worker.zig`; the browser packet changed the facade and
  dialog but could add no exports or `.def` entries; the token packet needed
  read-back where jobs finish and the client is destroyed, also `worker.zig`.
  All three allowlists now cover the job kind, worker transitions, exports,
  both `.def` files, facade and ABI tests.
- **The candidate filter was wrong and the count was unfounded.** `overview`
  is not a backend — it came from scraping rather than asking
  `config/providers` — so the wrapper set is eleven, not twelve. More
  importantly, removing wrappers does not prove the rest support writable
  bisync, so the plan now distinguishes *candidate* from *verified
  destination* and states no figure at all.
- **Per-field secret clearing was undefined.** The existing export
  `bk_cloudsync_creds_clear_secret()` takes no arguments, which cannot express
  which of several secrets to clear, and an empty value must be
  distinguishable from an omitted one since omitted means preserve.
- **P03-M01 depended on P02-M03**, which is now only rendering; validation
  moved to P02-M04.

The `remote_root` ambiguity is settled too: the catalogue does not describe the
remote path, so we supply a generic label, and the connection test verifies
**writability** — write a probe, read it, delete it — rather than only listing,
because a listable remote that refuses writes fails at the first sync instead.

## Sizing

31.0 MB fetched per platform, 84.3 MB installed. `strip` saves nothing —
30.4 MB of the binary is `__gopclntab`, which the Go runtime requires. A
trimmed s3+webdav build would be smaller but is the opposite of this plan's
goal, and costs a Go toolchain per platform.

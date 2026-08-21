# Next Packet

Resume at `phase-01-provider-catalogue/P01-M02-generic-schema.md`.

## Where implementation stands

**Phase 00 is complete** (P00-M01 through P00-M04) and **P01-M01 is complete**.
Every checkpoint is in its phase `MANIFEST.md` with the measurements; read
those before writing code, they carry findings the packet texts do not.

| packet | commit | result |
|---|---|---|
| P00-M01 bundled dependency | `319602e62` | rclone staged, executable, v1.75.0 |
| P00-M02 available out of box | `a70a3dab9` | found with `PATH` emptied; tests only |
| P00-M03 packaging and notices | `86c53c970` | notices shipped, package deterministic |
| P00-M04 package permissions | `6dccbc024` | `package-game` completes; exec bit survives |
| P01-M01 catalogue | `9aa47f7fb` | parse, cache, `matchProvider`, fetch job |

## Resuming on another machine

Branch `feature/cloud-profile-sync`, everything pushed. Toolchain is Zig
0.16.0, and `zig build` runs **from the repository root only** — anywhere else
it panics with FileNotFound.

Suites, all green at `b6018200c`:

```
zig build test-cloudsync-rc        -Dtarget=aarch64-macos -Dtest-mode=run   #  6
zig build test-cloudsync-daemon    -Dtarget=aarch64-macos -Dtest-mode=run   # 27
zig build test-cloudsync-abi       -Dtarget=aarch64-macos -Dtest-mode=run   # 10 + C++ consumer
zig build test-cloudsync-plan      -Dtarget=aarch64-macos -Dtest-mode=run   # 35
zig build test-cloudsync-catalogue -Dtarget=aarch64-macos -Dtest-mode=run   # 15
zig build test-cloudsync-worker    -Dtarget=aarch64-macos -Dtest-mode=run   #  8
zig build test-cloudsync-engine    -Dtarget=aarch64-macos -Dtest-mode=run   # 16
zig build test-package             -Dtarget=aarch64-macos -Dtest-mode=run   #  4
zig build verify-runtime           -Dtest-mode=run                          # 11
zig build test-streamio            -Dtarget=aarch64-macos -Dtest-mode=run   # 32
```

There are also `test-cloudsync-creds`, `-backup`, `-backend` and `-facade` from
the earlier sync plan.

**rclone no longer needs installing.** It is a hashed lazy dependency per
target; `zig build install-game` puts it in `zig-out/bin/rclone` and stages it
into the layout. Tests that want a live one honour `BK_TEST_RCLONE` and skip
cleanly without it:

```
BK_TEST_RCLONE=$PWD/zig-out/bin/rclone zig build test-cloudsync-daemon \
    -Dtarget=aarch64-macos -Dtest-mode=run
```

Expect a **cold-cache fetch of ~32 MB** on the first build for a given target,
including a test-only build — see the P00-M01 checkpoint for why that cannot be
gated.

## What the implementation learned that the packets do not say

- Zig's zip extractor ignores an archive's external file attributes, and both
  `Step.installFile` and `stage.zig` preserve their *source's* permissions, so
  the executable bit has to be set explicitly on the way into `zig-out/bin`.
- Lazy dependencies resolve during *configure*, before step selection, so
  `lazy` is conditional on the target but not on the step.
- `package-game` had never worked on macOS: the stage directory was
  `<root>/game`, which collides with the staged `Game` on a case-insensitive
  filesystem. Renamed to `package`.
- The packaged layout sits at **63,728 of zip's 65,535-entry ceiling** and
  2.88 GB of its 4 GiB. Overruns now fail loudly instead of truncating into an
  archive that opens and lies, but zip64 is the real fix and roughly 1,800 more
  files reaches the cliff.
- The catalogue fetch job deliberately skips `applyCredentials`: the catalogue
  describes the binary, not a remote, so a broken credential must not hide the
  list a player needs to fix it.
- The fixture is `tools/zig/fixtures/config_providers.json` — 264,837 bytes,
  five backends kept whole, captured from a live v1.75.0 daemon, with its
  provenance inside the file as a `_fixture` key that doubles as an unknown
  field the parser must tolerate.

## P01-M02 is the riskiest packet in the plan

It **replaces working, shipped code** — `creds.zig`'s two-arm union — and its
allowlist is only `creds.zig` and `creds_test.zig`. Five things cost the most
if missed, all of them recorded in the packet itself but worth flagging here:

1. The schema is `{backend, options, remote_root}`. `remoteParams` says the
   bucket is deliberately not an option because for S3 it is a path component
   carried by the alias target — migrating it as an option routes every sync at
   the account root instead of the bucket. Silent misplacement.
2. `remoteParams` must be **replaced outright**, emitting a flat `Name`-keyed
   map. Verified against v1.75.0: no non-empty `FieldName` differs from `Name`,
   no option name contains a dot, and a flat create round-trips unchanged.
3. The fingerprint is the **connection identity**, today `s3:{endpoint}/{bucket}`
   and `webdav:{url}`. Persist it explicitly, carry the legacy value over at
   migration byte-for-byte, and rotate only on backend, root, or the canonical
   non-secret option projection — a password-only edit must not rotate it.
4. Persist **two** flags, `secret` and `is_password`, from this first generic
   save. Phase 03's token read-back needs `is_password` and the catalogue cache
   may be gone by then.
5. Replace the 16 KiB caps. `load`'s failure mode is to return null, which
   would silently lose the credentials.

If files outside the allowlist fail to compile against the new API, that means
the allowlist is too narrow — **stop and report** rather than editing around it.

## Not implementable without help

- **Phase 04 acceptance** needs live services (MinIO, a WebDAV server, one
  OAuth backend) and human approval.
- **Windows and Linux** remain compile-verified only. The Linux
  `install-game` fails compiling the engine's C++ from a macOS host for lack of
  libc headers — pre-existing, reproduced on an unchanged tree.

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

## Tenth round — signing removed entirely

`P00-M04` is deleted and no Apple Developer identity is needed. The earlier
rounds treated signing as a requirement to be scoped; it is not a requirement
at all, which the binaries settle:

```
rclone (official v1.75.0):  Signature=adhoc, linker-signed  ->  spctl: rejected
Game (this project):        Signature=adhoc, linker-signed
```

Both are ad-hoc signed — which is what lets an arm64 binary execute on Apple
Silicon — and neither is notarized. The game already ships in that state, so a
bundled rclone adds no Gatekeeper condition the project does not already have,
and the platform plan excludes "installers, signing/notarization" outright.

The nested-Mach-O rule that motivated the gate only applies to a Developer ID
signed app. It survives as a one-line conditional in P00-M03: *if* the project
ever adopts signing, the bundled rclone must be signed before the archive is
built, because signing after archiving signs nothing. That is a constraint to
remember, not a packet to execute.

Sixteen packets. Nothing in the plan now requires a credential.

## Ninth correction round

- **UI preservation could reintroduce a value the save path had just cleared.**
  P01-M03 cleans invalid closed-field values out of the *stored* map, but the
  dialog preserved any typed value whose field survived a rebuild — so an
  invalid one would come back as an explicit submission and sail past a cleanup
  that only inspected what was already saved. The UI now preserves a value only
  when the field is editable or the closed value is still among its filtered
  examples, and the save path validates the **merged submission** rather than
  the old map.
- **A stale `region` example survived** at P01-M03's motivating bullet, calling
  it AWS-only four lines above the correction saying it names 39 vendors. It
  now names genuinely AWS-only options.
- **`matchProvider` had no owner.** Three packets needed it and P02-M01 was the
  first told to implement it, after P01-M03 already required it. It lives in
  `catalogue.zig` under P01-M01, with the operators tested there; P01-M03,
  P02-M01 and P02-M04 consume it.

## Eighth correction round

Edge cases rather than structure, and two of them corrected my own examples.

- **Vendor cleanup missed invalidated values.** Dropping options whose
  `Provider` stops matching does not cover an option that stays applicable
  while its *examples* change: a closed (`Exclusive`) field can keep a value
  the new vendor never offers. That value is now cleared, editable fields are
  left alone, and the test is synthetic because exactly one option across all
  69 backends is `Exclusive` today.
- **My AWS-to-Wasabi example was wrong.** `region` is not AWS-only — its own
  expression names 39 vendors including Wasabi, so it survives the switch;
  only its 153 provider-tagged examples change. The persistence test now uses
  a genuinely AWS-only option (`requester_pays`, `use_accelerate_endpoint`,
  `leave_parts_on_error`, `sts_endpoint`, `directory_bucket`), and the
  region-example test stays where it belongs, in P02.
- **The ABI carried more than the model needs.** P02-M02 passed the current
  option map while P02-M01 takes only backend and provider, which would have
  serialised freshly typed secrets across the boundary on every rebuild. Only
  backend and provider cross; the dialog preserves typed values by field name.
- **The field list omitted `Value` and `ValueStr`.** Added. `Groups` is *not*
  present on backend option records in v1.75.0 despite appearing in the
  general option-block documentation, so it is tolerated as an unknown field
  rather than modelled.

## Seventh correction round

- **Provider rebuilding stopped at the Zig model.** P02-M01 took a
  `selected_provider`, but the ABI exported "build a form for a backend" and
  the dialog had no vendor-change handler — so the filtering would have been
  correct in Zig and invisible on screen. The provider and the current option
  map now cross the boundary, the dialog rebuilds on change while preserving
  still-applicable typed values, and AWS-to-Wasabi is asserted both through the
  ABI and headlessly.
- **A vendor change is not a backend change.** Preservation was scoped to an
  unchanged backend, but switching S3 from AWS to Wasabi leaves the backend as
  `s3`, so options the new vendor never declares stayed stored and would keep
  being sent to rclone. (That round used `region` as its example; `region` is
  not AWS-only — corrected in the eighth round.) Options whose `Provider` no longer matches are now dropped on
  a vendor change, with the rest preserved.
- **Required validation ran against the raw catalogue**, which would block
  saving on a required field belonging to a different vendor. It now validates
  the active filtered form. The rule is recorded from rclone's source, since
  half-implementing it is easy: `MatchProvider` returns true when the
  expression is empty **or when the selected provider is empty**, so a backend
  with no vendor chosen shows all conditional fields rather than none.
- **The design document had drifted behind the packets** — no `Provider` or
  `ShortOpt` in its field list, `Hide` described as all-or-nothing, required
  validation with no mention of defaults or filtering, and a single secret
  flag. An implementer following it would have undone four corrections, so it
  now carries `MatchProvider` verbatim and rclone's visibility constants.

## Sixth correction round

Four correctness gaps, each measured against v1.75.0's catalogue.

- **Provider-conditioned fields were not modelled at all.** `Provider` appears
  on 35 options and **664 examples** — s3's `region` examples are AWS-only —
  and my field list missed it because I derived the list from s3's *first*
  option, which happens not to carry one. The form now filters options and
  examples by the selected provider and rebuilds when it changes, implementing
  the comma-list and leading-`!` semantics (72 distinct expressions, one of
  them naming 51 S3 vendors). Without this a Wasabi user is offered AWS
  regions that do not exist.
- **Required validation ignored defaults.** rclone accepts a blank required
  option when it has a non-empty default; three of the 66 required options do
  (`pixeldrain.api_url`, `iclouddrive.service`,
  `oracleobjectstorage.provider`), so treating blank as invalid would make
  those backends impossible to configure. Errors also name the field by label,
  not by `Help`, which is multi-line prose.
- **`is_password` was persisted a phase too late.** It arrived in P03-M03, so
  every credential written in phases 01 and 02 would have been unreadable
  safely once the catalogue cache was gone. Both `secret` and `is_password` are
  now written from the first generic save, and phase 03 only consumes them.
- **`Hide` is a bitmask.** Observed `{0: 915, 3: 36, 2: 13, 1: 4}`; bit 1 hides
  from the command line and bit 2 from the configurator. Omitting everything
  non-zero would wrongly hide the four `Hide=1` options.

Also: `ShortOpt` joins the modelled fields; the `FieldName` claim is narrowed
to *non-empty* field names, since blank means "use `Name`"; and the README no
longer claims every packet carries a failing test, since `P00-M04` is a human
release gate with no code.

## Fifth correction round

The previous round introduced an error of my own, corrected here, plus four
edge cases. All settled by running rclone v1.75.0 rather than by argument.

- **`FieldName` was the wrong key** for `config/create`, and the dotted-nesting
  rule invented a case rclone does not have. Measured: no backend option has a
  `FieldName` differing from its `Name`, no option name contains a dot, and a
  flat `Name`-keyed create round-trips through `config/get` unchanged. Backend
  configuration is keyed by `Name`; `FieldName` belongs to the global RC
  option-struct JSON.
- **Token read-back could destroy passwords.** `config/get` returns rclone's
  *stored* values, and an `IsPassword` field comes back obscured — sending
  `pass: "hunter2"` with `obscure: true` reads back as
  `tEvsvLJ9Bj-HwIyGtwp6i-2G2eqU8jQ`. Merging that as plaintext and re-obscuring
  on the next save double-obscures it and breaks authentication with the
  original gone. Read-back now covers non-`IsPassword` fields only; tokens
  qualify because on `drive` and `dropbox` they are `Sensitive` but not
  `IsPassword`, which is the discriminator.
- **A secret-only edit could rotate a migrated fingerprint**, since any option
  change triggered recomputation and the new digest will not equal the legacy
  `s3:{endpoint}/{bucket}` string. Rotation now compares backend, root and the
  canonical non-secret projection only; secret-only edits keep the stored value
  verbatim.
- **OAuth continuations must resend the whole envelope** — `name`, `type` and
  the full `parameters` map on every call, not just the continuation flags.
  The stub asserts it.
- **The signing requirement contradicted itself**: deferred to a human gate
  while the evidence still demanded `codesign --verify`, which an unsigned
  development build can never satisfy. P00-M03 now owns only the ordering
  constraint that keeps signing possible, and a new `P00-M04` owns the
  credentialed release gate — including that notarization needs `stapler
  validate` or `spctl`, not `codesign`.

## Fourth correction round

- **The schema change alone left `remoteParams` behind.** No bullet replaced
  it, so a generic schema would have had no way to reach rclone. It is now
  explicitly rewritten to emit every saved option. **The `FieldName` rule
  recorded here in that round was wrong and is corrected below**; the keying is
  a flat `Name` map. Tested on `sftp`, not only on the two backends we shipped.
- **Switching provider could carry credentials across.** Omission means
  preserve, and `user`/`pass`/`token` recur across backends, so a switch could
  have applied one service's password to another. Preservation now holds only
  while the backend is unchanged, with a cross-backend isolation test.
- **The fingerprint rule still needed forbidden knowledge** — "an obvious
  endpoint field" is field-name hardcoding, and the catalogue does not say
  which fields define a remote. One generic rule replaces it: keep the stored
  value while backend, root and options are unchanged; otherwise recompute
  from backend, root and a canonical digest of non-secret, non-default options.
- **P00-M03 was not executable.** The official archive has **no `COPYING`** —
  verified: it holds `rclone`, `rclone.1`, `README.html`, `README.txt` and
  `git-log.txt`, with the MIT text buried in the README. The notice now ships
  in a `Data/THIRD-PARTY-NOTICES.txt` we own rather than scraped at build time.
  Signing and notarization are excluded by the platform plan, so the packet
  records the required ordering and marks the credentialed steps a human
  release gate — and notes that `codesign --verify` does not establish
  notarization at all.
- **OAuth request casing was wrong.** rclone's documentation gives the request
  `opt` keys as lowercase `state`, `result`, `continue`, `nonInteractive`,
  while replies carry `State`, `Option`, `Error`. Posting the capitalised
  spelling is ignored, so the loop hangs rather than failing. `form.zig` is
  also in the allowlist now, since the reply's `Option` block shares the
  catalogue option's shape and the conversion should be shared, not copied.
- **Two verification paths lacked ownership**: the write probe must use
  `operations/copyfile` from a local temp file, because `rc.Client` posts JSON
  only and `uploadfile` needs multipart; and the final token read-back needs
  `worker_test.zig` so the teardown sequencing is tested rather than asserted
  by inspection.

## Third correction round

- **The generic fingerprint collapsed distinct remotes.** Deriving it from
  `backend` + `remote_root` would make two S3 services sharing a bucket name
  identical and give **every WebDAV configuration the same string `webdav:`**,
  since their root is empty — and it could not have preserved existing
  fingerprints across migration. The fingerprint is now persisted explicitly,
  carried over byte-for-byte at migration, and rotated on backend, root or
  non-secret connection identity. It is the connection identity, not the alias
  target; the two had been conflated.
- **`ensureCatalogue` had no worker ownership and no async contract.** Fetching
  spawns a daemon and makes an rc call, so it is a worker job with
  begin/poll/cancel/release exports, not a function the UI thread can call.
  `worker.zig` is in P01-M01's allowlist and P02-M03 polls it.
- **The writability probe contract was self-contradictory** — reuse
  `testConnection` unchanged *and* make it write; clean up on every path *when
  the failure under test is that deletion is refused*. Now: `testConnection` is
  explicitly extended, a typed outcome and UI text are added because the
  `Outcome` enum has no member for it, and an undeletable probe is reported by
  exact path rather than pretended away.
- **Remote-root requiredness cannot be derived**, so it is optional at form
  validation and the writability test discovers it with a real error from the
  service.
- **The MIT licence needs shipping, not noting.** rclone's `COPYING` is staged
  or folded into third-party notices, and its presence asserted.
- Three authoritative documents still contradicted the corrected design, and
  the staging instruction was wrong: `stage_runtime_files` is a name list built
  in `build.zig`, and `stage.zig` copies those names from `zig-out/bin`, so the
  extracted executable must be installed there first.

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

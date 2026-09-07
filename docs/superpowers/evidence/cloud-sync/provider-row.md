Ct
  investigation ("I caused this wipe twice during investigation and
  recovered both times by restoring the exact original `cloud.credentials`
  bytes"), not three — the first run's wipe, and one resave from a
  follow-on run before the document was restored; only the first
  restoration (before Step 1.3, above) is narrated here in full.

  **The trigger for the first, live occurrence was never reproduced.** Also
  reproduced on `P05`, which the earlier P04-M01 packet had already paired
  — restoring the wiped bytes afterward retired its pairing record (see the
  next finding), a real but separate consequence. Fix round 1 tried to
  force the original blank render back on this machine five separate ways,
  matching the evidence's conditions as closely as possible, and could not:
  (1) the tab route with `Cloud.Provider` already `s3` on `P05`; (2) the
  evidence's own Step 1.2 schedule verbatim, from `Off` through the full
  44-step walk to `s3`, on `P05`; (3) the same schedule with
  `cloudsync/providers.json` deliberately removed first, to force a cold
  catalogue fetch; (4) the same schedule on `P06` itself, still carrying
  its state from this evidence session; (5) with a trace confirming
  `CInterfaceOptionsSettings::StepLocal`/`BuildCloudList()` genuinely keep
  running the whole time the dialog sits on top of it — the settings
  screen really does stay "alive underneath," independently polling the
  same `NCloudSync` catalogue/credentials calls, which is the one concrete,
  verified structural difference from the direct main-menu route (no other
  screen is alive there) — prefill still worked correctly every time.

  **Fixed in `4a2d509c7`** — non-secret fields now carry the same "blank
  and untouched on the very backend already saved keeps the stored value"
  protection secrets already had (`bStoredSecret`), via a per-field
  `bTouched` flag now set on every real edit or example-cycle, not just a
  secret's. Verified on the same Cloud tab route, profile `P05`: reopening
  the saved `s3` backend prefills correctly, `Test connection` reaches
  `Connection OK`, and `OK` leaves the saved document — `backend`,
  `remote_root`, every non-secret option, and the `fingerprint` — byte-for-
  byte unchanged (`test-ok.png`, described above under Step 1.2's re-run).

  **Residual risk this alone did not close, and what covers it — fixed in
  `c45b43ef1`.** The round-1 fallback is only as good as `storedOptions`:
  on the very state that produced the original wipe — an empty stored
  snapshot, or (structurally identical to it, for this fallback) a
  backend mismatch — the fallback is a no-op and the same wipe could
  recur, since neither state gives it anything to fall back to.
  `c45b43ef1` closed this directly: an entirely untouched form refused to
  save at all when real credentials were on record
  (`NCloudSync::CredentialsPresent()`) and the backend was mismatched or
  the stored snapshot for it was empty — the same status-line refusal
  shape `SaveCredentials()` already used for unreadable credentials.

  **That guard was itself too broad — narrowed in `3381a7569`, then
  dropped entirely in `4d099afd6`.** The mismatch arm refused every
  untouched cross-backend save, but a player moving the Provider row to a
  zero-required-field backend (`drive`, `dropbox`, `onedrive`, `box`,
  every OAuth backend — 34 of the catalogue's 69 entries) legitimately
  starts setup by pressing `Test connection` on an intentionally blank
  form; that first save of `{backend: drive, options:{}}` has to go
  through so `ConfigBegin()` can run, and dropping the old backend's
  options wholesale on a backend switch is the designed cross-backend
  isolation, not the bug the guard exists to catch. `3381a7569` therefore
  narrowed the condition to `bSameBackend && CredentialsPresent() &&
  storedOptions.empty()` — and that was still wrong, in the opposite
  direction: it false-refused a *working* setup. A configured OAuth
  backend's steady state is exactly `options:{}` with the token living in
  `secret_options`, because every field of `drive`/`dropbox`/`box`/
  `onedrive` is `Sensitive` or `IsPassword` and none of them reaches the
  non-secret snapshot. An empty `storedOptions` is a legitimate state,
  not evidence of a broken view, so re-testing or OK-closing perfectly
  good credentials answered `Could not save: nothing was typed; the saved
  credentials were left as they are` with nothing wrong at all.

  **What replaced it — skip-write, in `4d099afd6`.** The honest signal
  was never the snapshot; it is that nothing was typed. `SaveCredentials()`
  now returns success *without writing anything* when the form's backend
  is the stored one, credentials are on record
  (`NCloudSync::CredentialsPresent()`), and no field is `bTouched`: an
  untouched same-backend form has nothing to say. The document on disk is
  already the authority, this form at best mirrors it, and writing from it
  could only lose information — which is precisely the wipe this thread
  exists for. Nothing is lost by not writing, because both callers read
  that document as their very next step: `BeginConnectionTest()` probes it
  through `ConfigBegin()`, and `IMC_OK` closes the dialog. An untouched
  cross-backend save still writes (the player's deliberate switch, and the
  `{backend, options:{}}` document first-time OAuth setup needs), and any
  touched form writes as before with the round-1 preserve fallbacks. The
  `nothing_to_save` status text went with the refusal.

  Verified against the rebuilt binary, profile `P05`, the direct
  `cmd=0x100e0104` route, `profiles/cloud.credentials` backed up and
  restored around every destructive step: (1) untouched reopen on the
  configured `s3` — `Test` reaches `cloud credentials: connection test ok`
  with `Connection OK` on the status line and the document byte-identical
  (`md5 ca3c718f…` before and after, i.e. no write happened), then `OK`
  closes to the main menu with the document still byte-identical; (2) the
  document forced into the wipe-adjacent state (`s3` stored, `options:{}`,
  empty fingerprint) — no refusal now: the probe runs against that blank
  document and reports `connection test failed: … ListBuckets …`, a
  classified failure, and the file stays byte-unchanged after both `Test`
  and `OK`; (3) an untouched `drive` row over the real `s3` document still
  writes `{"backend":"drive","remote_root":"","options":{},…}` and reaches
  rclone's `config_shared_client_id` OAuth question; (4) a touched
  same-backend edit persists — typing into `region` and pressing `OK`
  wrote the new region while `remote_root`, `provider`, `endpoint` and
  both stored secrets came through untouched; (5) a typed webdav form
  still saves (`{"backend":"webdav",…,"options":{"url":"https://example.com/dav"}}`).

  **Residual risk accepted.** Skip-write still distinguishes same-backend
  from cross-backend purely by comparing the Provider row's current value
  against the stored document's `backend` field. A trigger that
  spuriously flipped the row's value out from under the player (without
  touching the document) would present as a legitimate backend switch and
  bypass the invariant — but that requires the option store itself to
  misreport the row, a failure mode distinct from, and not covered by, the
  dialog-state bug this closes. And the honesty from the start of this
  finding still stands: the *first* blank render was never reproduced, so
  none of this is a diagnosis of the trigger. It is the narrower claim
  that an untouched form can no longer destroy what is on disk, whatever
  made it come up blank.

  `c45b43ef1` also closed a second, related gap the round-1 fallback
  introduced on its own: cycling the `provider` field within the same
  backend (`RebuildForm( true )`) deliberately drops a `droplist_closed`
  value the new vendor does not offer, but left the replacement field
  looking untouched — the very state the round-1 fallback would then
  resurrect the dropped value into, from `storedOptions`, on save. The
  rebuild path now marks that replacement field touched when a value is
  dropped this way, so the drop sticks. This one could not be verified
  organically: the bundled rclone's own s3 catalogue never marks a single
  field `Exclusive` (confirmed across all 69 cached backends — the one
  field anywhere that is, `azureblob`'s `delete_snapshots`, belongs to a
  backend with no `provider` sub-field to cycle at all), so
  `droplist_closed` never actually occurs through the live UI today.
  Verified instead with a temporary, since-removed debug hook
  (`BK_FORCE_CLOSED_TEST`) forcing the `region` field's widget to
  `droplist_closed` with an example set that excludes whatever is
  currently stored: with the fix reverted, cycling the `provider` field
  away from `Minio` and saving resurrected `"region":"us-east-1"` into a
  vendor that (by the forced example set) does not offer it, confirmed
  writable, the exact resubmit the code's own comment rules out; with the
  fix restored, the identical steps produced a document with no `region`
  key at all. Both runs used the same starting document and the same
  cycle-then-save steps, differing only in whether the fix was present.
- **Side effect: `P05`'s pairing record was retired.** The credential wipe
  above rotated the stored fingerprint (from `s3:bk-saves#07fd8363...` to
  `s3:#6e88edc9...`, since an empty `remote_root`/`options` hashes
  differently), and per `129dcc166`'s designed behaviour a credentials save
  naming a different fingerprint retires pairing records that named the old
  one — `cloudsync/state/P05.json` (present at the start of this session,
  `146` bytes, from P04-M01's own earlier work) is gone after this packet's
  runs, with no `retired`-style archive found under `cloudsync/`. This is
  the system correctly reacting to what looked like a real credential
  rotation; restoring the byte-identical original document afterward did
  not restore the retired record (retirement does not appear to be
  reversible by restoring old bytes — a new pairing dialogue is what the
  design intends here). `P05`'s saves on disk are untouched; only the
  already-paired fast path is gone, and `P05` would simply re-pair
  (`NotPaired → pair`) on its next startup sync, same as any other
  fingerprint change.
- **The Provider row's `key=RIGHT`/`key=LEFT` navigation needs a checkpoint
  `shot` roughly every ten steps to stay accurate over a long walk.** At an
  8-frame cadence, 44 presses glitched off the Cloud tab entirely. At a
  24-frame cadence with no intermediate checkpoints, a 44-step walk drifted
  (one run landed on `shade`, three past `s3`); the same 24-frame cadence
  with a verifying `shot` every ten presses landed exactly on the computed
  alphabetical position at every checkpoint, repeatably. Also: `Cloud.Provider`
  is `InstantApply` and persists to `config.cfg` on a raw process `exit` with
  no `OK` needed — a launch's starting row position is always wherever the
  previous launch left it, not `Off`, unless explicitly reset with
  `set=Cloud.Provider=Off`. Both facts are harness/evidence-gathering notes,
  not game defects, and are recorded here for whoever next drives this UI
  headlessly.

## Human approval

**Pending.**

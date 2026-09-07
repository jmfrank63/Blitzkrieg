# Phase 02 — Generic Credentials Form

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** One renderer for every backend, driven entirely by the catalogue.

| Packet | Depends on | Owns |
|---|---|---|
| P02-M01 | P01-M04 | form model derived from the catalogue |
| P02-M02 | M01 | form exports |
| P02-M03 | M02 | the dialog renders the model |
| P02-M04 | M03 | validation and connection test for any backend |

Exit: an arbitrary destination backend is configurable and testable with no provider-specific code.

P02-M01 macOS checkpoint: `test-cloudsync-form` 8/8, written failing-first
over the committed fixture; the full sweep stays green and
`x86_64-linux-gnu` / `x86_64-windows` compile. Commit `35428f79a`.

Measured from the fixture and asserted (empty provider, +1 for the
remote-root field in basic): s3 15 basic / 61 advanced of 78 options,
webdav 6/10 of 15, sftp 14/34 of 48, drive 5/41 of 52 — the differences
against raw counts are the configurator-hidden options (3, 0, 1, 7) and
drive's one hidden basic option. Region examples: 26 under AWS, 2 under
Wasabi, of 153 total.

Findings the packet text does not carry:

- **No Exclusive option exists in the five fixture backends** (the plan's
  "exactly one across all 69" is outside them), so the closed-droplist rule
  is covered synthetically, as the later packets already planned for the
  vendor-cleanup rule.
- **A non-text `kind` still renders `.text` by the widget rule** — the
  packet's rule names only masked/droplists/text — but `Field.kind`
  carries the catalogue classification (`boolean`, `integer`, `number`) so
  P02-M03's renderer can refine the widget without the model guessing.
- The remote-root strings (`remote_root_label`, `remote_root_help`) are
  the canonical fallback text; the renderer may localise them, and they
  are public constants so the dialog and the model cannot drift.
- Form slices borrow from the catalogue: the catalogue must outlive the
  form, which the exports in P02-M02 must arrange (the ABI serialises, so
  nothing borrowed crosses the boundary).

P02-M02 macOS checkpoint: the C++ consumer failed first on the one missing
symbol, then abi 10/10 + consumer green natively and with a live rclone —
where all four real backends build across the boundary and the s3 rebuild
under AWS versus Wasabi changes the region example set (`us-east-2`
present, then absent), the same assertion the Zig tests make, repeated
across the ABI because a boundary that drops the provider argument passes
every Zig test and still renders the wrong form. Facade, form, and the
full sweep green; both cross-targets compile; `install-game` builds.
Commit `3135a2fed`.

Findings the packet text does not carry:

- **The wire format omits a per-field `advanced` flag on purpose**: the
  split into `basic` and `advanced` arrays *is* the encoding, and a flag
  that could disagree with the array a field sits in would be a second
  source of truth.
- **An unknown backend distinguishes its two causes** in the error text:
  an empty cache says "no provider catalogue is cached; fetch it first" —
  the actionable half — while a populated cache says the backend does not
  exist.
- The real s3 form under an empty provider is far too large for a stack
  buffer — 153 region examples with help text among 75 visible options —
  so the consumer's live branch reads through a 256 KiB static buffer,
  which the run proved sufficient. The required-size contract is what a
  caller without such a bound relies on.

P02-M02 amendment (`1e762cf92`, during P02-M03): the offered destination
list crosses as `bk_cloudsync_catalogue_destinations` — P01-M04's
checkpoint had assigned the `offeredBackends` chain to this packet, but
the bullets never said so and the provider chooser cannot render without
it. Recorded in the packet.

P02-M03 macOS checkpoint: the dialog renders the form model — one
renderer, no per-backend field set. Verified headlessly at 1024x768
against the real fetched catalogue
(`evidence/cloud-provider-coverage/credentials-form/`, eleven captures):
s3, webdav and drive with visibly different field sets; a one-step vendor
switch (US3 → Wasabi) preserving a typed endpoint while the region
examples change from AWS's `us-east-2` set to Wasabi's two; the advanced
split toggling 60 options in and the window scrolling into them; the
chooser stepping webdav → yandex with a fresh option set (cross-backend
isolation on screen); the fetching state; and the explained
missing-catalogue state whose chooser doubles as the retry. All suites
stay green. Commit `fb858fe78`.

Findings the packet text does not carry:

- **The screen's click routing lives in `Data/UI/CloudCredentials.lua`**,
  a sibling the packet's allowlist does not name: only ids listed there
  become messages, so every new button was silently dead until the router
  learned them. Read as part of the screen unit the XML names — both load
  through the same `Load("ui\\CloudCredentials")` — and recorded here
  rather than stopped over: no other packet owns it and no other file is
  affected.
- **Droplists render as edit-plus-cycle-button**, not `IUIComboBox`: that
  widget has no usage anywhere in GameTT or the UI data, and an unproven
  widget under a packet this large is risk without a reader. The cycle
  button steps the filtered examples and surfaces each example's help in
  the status line — the only per-value documentation the catalogue has.
- **The vendor-switch evidence pairs US3 → Wasabi, not AWS → Wasabi**: the
  harness cannot clear an edit box (no backspace key action), so the
  switch is driven by the cycle button, and stepping AWS→Wasabi crosses
  IDrive — the one vendor whose s3 form legitimately drops `endpoint`
  (its 52-vendor expression omits exactly IDrive), which correctly clears
  the value en route. One step from the adjacent vendor exercises the
  identical preserve rule; the AWS/Wasabi region difference itself is
  asserted by the P02-M01/M02 tests and captured in the AWS run.
- **The P01-M02 interim guard is gone**: the dialog speaks the generic
  schema natively now (its own recursive JSON reader — the flat scan
  cannot represent arrays of objects). A present-but-unreadable document
  still refuses to save rather than overwriting with blanks.
- The headless frame rate in this rig is roughly 4 fps, so the
  catalogue-fetch window is one or two frames — the fetching-state
  capture sits two frames after the open click.

P02-M04 macOS checkpoint: validation and the write-probe connection test,
generic for any backend. Form 9/9 (failing-first on the must-fill rule),
engine 19/19 with a live rclone — a writable webdav serve passes the full
probe round trip and is empty afterwards, a `--read-only` serve
classifies `remote_unwritable`, the wrong root stays `remote_missing` at
the listing. Headless through the game (four new captures in
`evidence/cloud-provider-coverage/credentials-form/`): a blank required
`url` refused by label before any network call, Connection OK against a
local serve through the whole generic chain, the unwritable text with
the service's own words, and the root typo failing rather than
succeeding against the account root. All suites green, both
cross-targets compile, `install-game` builds. Commit `d37fcb73d`.
**Phase 02 exit reached**: an arbitrary destination backend is
configurable and testable with no provider-specific code.

Findings the packet text does not carry:

- **The `remote_unwritable` outcome text lives in
  `Data/Textes/UI/CloudSync/`**, beside the other outcome texts — the
  packet's allowlist names the sibling `Data/Textes/UI/CloudCredentials`
  and its bullet says the text resource is allowlisted, so the intent is
  explicit and only the directory was off. Recorded like P02-M03's Lua
  router rather than stopped over.
- **The leading-tag mapping exists in three copies** (the dialog,
  `MainMenu.cpp`, `InterfaceCloudBackups.cpp`); only the dialog's was
  extended. The other two fall back to their generic "failed" text, and
  only the connection test can produce the new tag today — sync failures
  classify from the run log — so they were left deliberately, being
  outside the allowlist.
- **The probe's local source file lives in the platform temp directory,
  resolved inside the engine** (libc's environ — a loaded library never
  sees `main`'s): `testConnection`'s signature could not grow a
  directory parameter without touching `worker.zig`, which the packet
  does not allow.
- **A read-only webdav serve refuses the write with `404 Not Found`** —
  the "whatever status it wears" rule paid off immediately; classifying
  the probe step by status pattern would have called it a missing root.
- **The delete-refused branch is code-covered, not live-tested**: no
  local serve mode accepts writes while refusing deletes (permission
  tricks on the served directory break the upload too). The
  leftover-probe report — outcome text plus the exact file name on the
  status line — is exercised by reading the code and by the suffix
  plumbing the read-only capture shows.
- The must-fill fold also governs the star rendering: a required option
  with a catalogue default loses its `*` on purpose — blank is
  satisfied, and starring it would demand what rclone does not.
- The harness `text=` action cannot carry a URL (colons separate the
  schedule), so the evidence seeds credentials documents and clicks
  Test, the same substitution P02-M03 recorded for typing.

P02-M04 / phase-02 review follow-up (`04a8af1f2`, `9dc1c70a8`): two
findings. `runIdTimestamp` bounded days at a flat 1–31, so a trash name
wearing Feb 31 — which `runId` can never emit — parsed (`daysFromCivil`
normalises it into March) and became prune-eligible, against the
delete-only-what-we-created guarantee; the day now checks against the
month's real length, leap years included. And the engine's redacted
200-line support tail never crossed the ABI: the snapshot truncates at
512 bytes, `bk_cloudsync_error` reads only the snapshot, and the sync
and connection-test paths composed into a snapshot-sized buffer whose
overflow fallback dropped everything but the outcome tag. The worker
now keeps the full text beside the snapshot, the compositions happen at
full length, and `bk_cloudsync_error_detail` (@30, facade `ErrorDetail`)
hands it out under the required-size contract — `bk_cloudsync_error`
stays the one-line summary. Worker suite is 10; the detail is the
worker's most recent failure, faithful per handle because the worker
runs one job at a time (documented on the export).

P02-M03 review follow-up (`deb635255`): `recordError` scrubbed the run-log
tail but stored the rc error message raw — and rclone repeats the
filesystem name, connection-string secrets included, in the message
itself; a connection-test failure has no log, so the text was the whole
exposure through `bk_cloudsync_error`. The marker scan is now
`redactedText`, applied to everything `recordError` stores
(`redactedLogTail` composes it after the tail cut), an allocation failure
drops the text rather than keeping it raw, and a new engine test drives a
secret-bearing error message through the stored text with and without a
log. Engine suite is 17 now.

P02-M03 review follow-up 2 (`6ac8aea67`): the static marker table names
S3/WebDAV-era keys, while the generic schema lets any catalogue option be
secret — a backend's `client_secret` in an rclone error would have
crossed `bk_cloudsync_error` unchanged. The worker now hands the engine
what the loaded credentials designate, at every job start (the cadence
the daemon's config is re-applied on): secret option names struck as
`name=` markers like the built-ins — which catches the obscured forms
this code cannot know by value — and the plaintext values struck
wherever a server or log echoes them, with a four-byte floor below which
a "secret" is unrecognisable as a leak and striking it would censor
arbitrary letters. An out-of-memory building the set refuses the job.
`creds.Option.withheld` is pub now — the worker reuses the canonical
predicate instead of re-deriving it. Engine suite is 18.

P02-M02 review follow-up (`d76d1d8b2`): the daemon's fifteen-second
`waitReady` never observed the worker's cancel flag, so a cancel or
shutdown landing during daemon startup sat out the whole readiness window
— contradicting `destroy`'s documented bound. `waitReadyAbortable` checks
an abort signal after every probe (recording no failure: nothing is wrong
with the daemon when the caller leaves), the worker passes its cancel
flag, and a new worker test drives a never-ready daemon script and asserts
the job settles as `Cancelled` well inside the window. Worker suite is 9
now.

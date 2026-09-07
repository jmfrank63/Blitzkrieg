# Phase 03 — OAuth Backends

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Reach the consumer clouds that need a browser.

| Packet | Depends on | Owns |
|---|---|---|
| P03-M01 | P02-M04 | rc config state machine |
| P03-M02 | M01 | browser launch and callback |
| P03-M03 | M02 | token storage and refresh |

Exit: one OAuth backend authorises and completes a sync.

P03-M01 macOS checkpoint: `test-cloudsync-oauth` 7/7, written failing-first
— the driver replays a captured v1.75.0 drive sequence
(`client_id_warning` → `*oauth-islocal,…` → `*oauth-authorize,…` → done)
against a stub that asserts every request envelope, and a live-gated test
walks the real machine to the token question. Worker 10, form 9, engine
19, abi 10 + consumer, facade green; both cross-targets compile;
`install-game` builds. Commit `88791fe66`.

P03-M02 macOS checkpoint: browser consent works end to end. Oauth 8/8
(async stubs incl. the consent card and `config/oauthstop`), abi 10 +
consumer with a live walk of drive to the token question and back via
cancel, all other suites green, both cross-targets compile,
`install-game` builds. Evidence
(`evidence/cloud-provider-coverage/oauth-consent/`): a recorded
consent-to-completion run through the production dylib against a local
consent+token provider (the dance lands its token in `rclone.conf`), the
cancel path at the consent screen, a daemon log scanned clean of tokens,
secrets and codes, and headless captures of the question takeover and
the waiting state. Commit `31df1d7da`.

P03-M03 macOS checkpoint: tokens persist and refresh. Creds 19/19
(failing-first on `applyReadBack`), worker 12/12 — the canned-server test
covers read-back after a failed job, the password untouched
byte-for-byte, the catalogue-cache-deleted classification fallback, and
the teardown sequencing (a second `config/get` answers a newer token
that must be on disk after `destroy`); a live test drives a revoked
refresh token to `auth_failed` with the token absent from the error text
and `rcd.log`. The consent harness shows the survival end to end: run B
now asks `config_refresh_token` instead of demanding a second consent,
and `rclone.conf` still holds the token after the next job's apply — the
P03-M02 wipe finding closed. All suites green, both cross-targets
compile, `install-game` builds. Commit `36472b80e`. **Phase 03 exit
depends on a live sync over an OAuth remote, which needs a real account
— phase 04 acceptance owns that.**

Findings the packet text does not carry (P03-M03):

- **The wipe is closed by round trip, not by prevention**: the next
  job's `config/create` still replaces the section, but the document now
  carries the token and re-supplies it as a parameter — which is why the
  machine's next question is `config_refresh_token`, a state the opaque
  driver had never seen and carried without change.
- **A new field the catalogue calls IsPassword is never imported**:
  rclone returns it obscured and the plaintext was never ours — the
  packet's double-obscure warning applied one step further than its
  bullet states.
- Read-back runs after failed jobs too: the refresh happens before the
  operation that then failed, and the deferred call in `execute` covers
  every exit path.
- The classifier gains `couldn't fetch token` and `invalid_grant`
  (captured live from a revoked-grant refresh); `engine_test.zig` is
  outside this packet's allowlist, so the patterns are exercised by the
  live worker test instead of the classification fixture table.
- `daemon.zig` and `build.zig` turned out untouched: the teardown
  read-back sequences inside `runWorker` before the deferred session
  deinit, and the existing worker suite carries the new tests.

Findings the packet text does not carry (P03-M02):

- **The dance dies with its request's connection** (measured): rclone
  aborts the pending auth when the blocked `config/create` POST drops,
  so a synchronous continuation under the per-POST deadline kills the
  sign-in at the deadline. Every exchange therefore runs as an rc
  `_async` job — the reason `oauth_test.zig` joined the allowlist by
  amendment.
- **rclone opens the browser itself when it can**: `config_is_local=true`
  execs the platform opener from the daemon, so on a desktop both rclone
  and the dialog may open the consent page (two tabs of the same URL at
  worst — the second goes stale harmlessly). Headless or PATH-less
  daemons cannot, which is exactly when the parked card and the
  dialog-side launcher carry the flow.
- **rclone's own NOTICE writes its auth URL into `rcd.log`** as the
  manual fallback hint. The state nonce in it is single-use and dead
  once the dance settles; the tokens, client secret and authorization
  code appear nowhere in the log (scanned). Our layers never log the
  URL.
- **`applyCredentials` had to go `nonInteractive`**: a plain
  `config/create` for an OAuth backend runs the whole dance inside the
  call — headless, blocking, killed by the POST deadline ("cloud
  credentials could not be applied"). Parameters persist either way
  (verified against v1.75.0).
- **`config/create` replaces the whole section: the token is wiped by
  the next `applyCredentials`** (measured — the evidence harness shows
  run B's apply erasing run A's token). Inside one config job the order
  is safe (apply, dance, test); across jobs this is precisely P03-M03's
  token-storage problem.
- **The facade's `ErrorDetail` passed the facade handle straight to the
  library** — found while wiring the new wrappers; it worked only
  because first-job slot numbers coincide. Routed through the job table
  like every other per-handle wrapper.
- The dialog tracks question mode in two file-scope statics reset on
  `StartInterface` — the header is outside this packet's allowlist, and
  one credentials dialog exists at a time.
- Drive marks `client_id` *Sensitive*, so a seeded document must list it
  in `secret_options` or the dialog's save correctly drops it
  (nothing-stored, nothing-typed) — it is a masked field like any other
  secret.
- The consent-waiting and question texts use `TextOrFallback` English
  fallbacks; the `Data/Textes` files are outside this allowlist and can
  land with P03-M03.

Findings the packet text does not carry (P03-M01):

- **The captured fixture's completion and error shapes come from adjacent
  captures**, not from finishing a real OAuth dance (that needs a human
  and a browser): the done reply is the same daemon completing a webdav
  create in one step, and the in-band error is its answer to a garbage
  token — both byte-shapes of the same machine, recorded in the test's
  module doc.
- **A `client_id` parameter legitimately changes the machine**: drive
  skips its shared-client-id warning when one is set, which the live test
  discovered — captures and live walks must agree on the parameters they
  send.
- **The reply's Option block is parsed by the catalogue parser through a
  synthetic one-option wrapper** rather than a second Option-JSON mapping
  in oauth.zig — catalogue.zig is outside this packet's allowlist, and
  the wrapper reuses it verbatim; `form.fieldFromOption` is the shared
  conversion the packet asked for.
- **`cloudsync.zig`'s undo-availability busy list gains
  `.awaiting_input`** — the single exhaustive switch over the worker
  state enum this packet extends, outside the allowlist but with exactly
  one correct arm (a parked question holds the operation slot). Recorded
  here like P02-M03's Lua router.
- The wait for an answer is bounded by cancel and destroy only — there is
  no honest timeout for a person reading an OAuth page — while every rc
  exchange inside the flow keeps the client's per-POST deadline. Stale
  answers are dropped when a new question parks.
- In-band error text goes through `engine.redactedText` with the
  session's credential-derived redactions before it can reach the
  question JSON or a status line: rclone echoes rejected parameters.

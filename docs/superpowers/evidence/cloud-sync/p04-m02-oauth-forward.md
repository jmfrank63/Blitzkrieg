# P04-M02 — OAuth and forward compatibility

Measured 2026-08-25 on Apple Silicon macOS, the same release build and
headless BK_AUTO_UI rig as P04-M01 (`zig-out/game/macos/arm64/release`,
profile `P04`). The forward-compatibility half is done; the OAuth half
needs a real consumer-cloud account and a human at the consent screen —
the procedure is at the end, results to be recorded here. Captures live
in `p04-m02/`.

## Forward compatibility: a newer rclone, no game change

The staged binary was swapped for **rclone v1.76.0-beta.10198.8869a848f**
(the current beta; bundled is v1.75.0) — the exact shape of a newer game
package shipping a newer rclone, with the game binary and libraries
untouched throughout.

**The cache refreshes on a version change, both directions.** Opening the
credentials dialog with the new binary refetched the catalogue: the cache
stamp went `v1.75.0` → `v1.76.0` and the content genuinely changed.
Restoring v1.75.0 later refetched again, stamp back to `v1.75.0`. No
stale catalogue was ever served across a version change.

**Real new catalogue content reached the screen with no rebuild.** No
current upstream release adds a whole backend (v1.75.0 → v1.76.0-beta
adds and removes none of the 69), but the beta does change real content:
MEGA S4's s3 endpoints were re-regioned (14 new `*-1`/`*-2` endpoint
examples, 11 old ones gone). Through the unmodified game:
provider `Mega` cycled in, the endpoint example-cycle offers
`s3.eu-luxembourg-1.megas4.com` with its "Mega S4 Luxembourg 1" help —
values that exist only in the beta's catalogue
(`beta-mega-endpoint.png`, discovery line showing rclone 1.76.0).

**A backend the game has never heard of is offered and configurable.**
Since upstream currently offers no genuinely new backend, this half is
demonstrated with a synthetic one: a `bkfuture` backend (required
`endpoint`, `Sensitive`+`IsPassword` `token`) appended to the served
catalogue document. The game offered it in the chooser between `b2` and
`box`, rendered its form with the required star and the masked token,
and saved it entirely through the generic path:

```
backend: bkfuture, options: {endpoint: https://future.example.net},
secret_options: [token], password_options: [token]
```

(`beta-bkfuture-form.png` shows the stored profile rendering — prefilled
endpoint, stored-secret placeholder.) The destination filter is a
deny-list of the eleven wrappers, so unknown names pass by construction —
there is no backend allowlist anywhere in the game. What this substitutes
for is only the *fetch* of a truly new backend, and the fetch chain is
the piece proven above with real binaries; when upstream next ships a new
backend, one re-run of this packet closes the literal case.

**A backend removed upstream does not break a configured profile.** With
`bkfuture` configured, v1.75.0 was restored (its refetched catalogue has
no such backend):

- The credentials document survived intact — backend, options and secret
  flags all load with no catalogue knowledge of the backend
  (`secret`/`is_password` are persisted per field since P01-M02).
- The dialog opens on `Service: bkfuture` with the plain reason on the
  status line — "the catalogue has no such backend" — and **one chooser
  press walks to a real backend** with its full form
  (`downgrade-unknown-backend.png`, `downgrade-chooser-escape.png`).
  The empty row set cannot save, so nothing can overwrite the stored
  document from that state.
- A startup sync against the unknown backend fails classified —
  `sync failed: cloud credentials could not be applied to the daemon` —
  and the game runs on normally.

One fix fell out of this packet: the dialog originally collapsed into
the missing-catalogue state here, whose chooser is the fetch-retry — the
retry re-derived the same unknown-backend form and the player was
trapped. Fixed in `3a07ec9ad` (owner approved going outside the
allowlist); the escape run above is the post-fix behaviour.

## OAuth — pending, needs a human and a real account

Everything machine-verifiable around OAuth is already in evidence: the
full consent dance, token storage, restart persistence and cancel path
ran against a local provider through the production library
(`evidence/cloud-provider-coverage/oauth-consent/`, P03), and P04-M01
plus the runs above prove the generic form and sync engine on live
services. What remains is the real-account run this file must record:

1. Build/run the release game, open Settings → Cloud → credentials.
2. Choose `drive`, `dropbox` or `onedrive` in the chooser. Leave
   `client_id`/`client_secret` empty to use rclone's shared client.
3. Test connection — the config machine asks its questions in the same
   rows (OK answers), then opens the system browser for consent. Finish
   signing in; the dialog shows Connection OK.
4. OK to save; enable sync; play/save; confirm the sync finishes and the
   remote holds `profiles/<name>/`.
5. Quit, relaunch, confirm the next sync completes **without a new
   consent** — the refreshed token was read back into
   `cloud.credentials` (P03-M03) and survives the restart.
6. Record backend, dates, and the sync outcomes here.

## Human approval

**Pending.** To be recorded here with the OAuth run.

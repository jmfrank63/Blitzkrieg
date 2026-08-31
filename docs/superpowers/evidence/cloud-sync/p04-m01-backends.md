# P04-M01 — static-credential backends end to end

Measured 2026-08-25 on Apple Silicon macOS (Darwin 25.5.0), release build
(`zig build install-game -Dtarget=aarch64-macos --release=fast`), run from
`zig-out/game/macos/arm64/release`, bundled rclone v1.75.0, dedicated test
profile `P04` (`./Game -windowed -profile=P04`). Every backend was
configured **entirely through the generic credentials form**, driven
headlessly with `BK_AUTO_UI` (`cmd=0x100e0104` opens the dialog; the
chooser, the example-cycle buttons and the row edits are the same messages
and clicks a player produces), and every sync ran through the shipped
game's own startup path — `cloud sync: startup sync begun for "P04"` /
`sync finished (paired|synced)` in the game's trace, never through the test
suites. Captures live in `p04-m01/`.

## The three services

| backend | service | endpoint | remote root |
|---|---|---|---|
| s3 | MinIO RELEASE.2025-09-07T16-13-09Z (`minio server`, darwin-arm64) | `http://127.0.0.1:19100` | bucket `bk-saves` |
| webdav | `rclone serve webdav` v1.75.0, ownCloud mtime semantics (`vendor=owncloud`) | `http://127.0.0.1:19200` | *(empty — account root)* |
| sftp | `rclone serve sftp` v1.75.0, password auth | host `127.0.0.1`, port `19300` | path `bk-root` |

Three deliberately different shapes: an S3 bucket root, a rootless WebDAV
account, and an SFTP path — with three different secret layouts
(`access_key_id`/`secret_access_key` both `Sensitive`; `user`+`pass` with
`pass` an `IsPassword`; `host`/`user`/`pass` where even the host is
`Sensitive`). Side-channel divergence and verification used a **separate**
rclone process with its own config (`--config rc.conf`), never the game's
daemon.

## Form entry, per backend

The chooser walked the filtered destination list to each backend (43 steps
to `s3` from a fresh install's `azureblob`, 11 more to `webdav`, 48 to
`sftp` — the stored backend is where the dialog reopens). Values were typed
through the real text path (`text=`), including full URLs:
`http://127.0.0.1:19100` survives intact — colons pass through the
schedule parser, which splits an entry only at its first colon. The
`provider` field is the exception: it was stepped with its example-cycle
button (30 steps to `Minio`), not typed — see the finding below. Required
enforcement, masked secrets, the advanced split and the scroll window all
behaved as P02 shipped them; `Test connection` ran the interactive config
machine to `Connection OK` on screen for all three
(`cloud credentials: connection test ok` in the trace), and OK saved.

The saved documents came back redacted exactly as designed:

- s3: options `provider=Minio, region=us-east-1, endpoint=…:19100`, root
  `bk-saves`, `secret_options=[access_key_id, secret_access_key]`,
  `password_options=[]`
- webdav: options `url=…:19200, vendor=owncloud`, root empty,
  `secret_options=[user, pass]`, `password_options=[pass]`
- sftp: options `port=19300`, root `bk-root`,
  `secret_options=[host, user, pass]`, `password_options=[pass]`

No secret value ever appears in a loaded document (`has_secret: true`
stands in), and a backend switch dropped the previous backend's options
wholesale — cross-backend isolation held on screen and in the file.

## The phase-02 cycle, identical for all three

For each backend, in order: **pair** (startup sync of a profile holding
`quick.sav=v1`, `f-local.sav`, `f-remote.sav`, `pad.sav`) — the remote
gained all four plus the `.bkprofile` sentinel, mtimes preserved to the
service's precision; **diverge** (remote `quick.sav=v2-remote`, 3 s later
local `quick.sav=v2-local`; delete `f-local.sav` locally and
`f-remote.sav` remotely); **converge** (next startup sync); then a
**remote-newer** round (`v3-local`, 3 s later `v3-remote`).

Results, identical on MinIO, WebDAV and SFTP:

- Converged to `v2-local` on both sides — the newer side won.
- The loser preserved as `quick.sav.conflict1` beside the winner.
- The remote-side delete recoverable from the **local** trash:
  `profiles/P04/.cloudsync-trash/<runid>/saves/f-remote.sav` = `keep-r`.
- The local-side delete recoverable from the **remote** trash:
  `trash/P04/<runid>/saves/f-local.sav` = `keep-l` — a sibling of
  `profiles/`, and `profiles/P04/trash` does not exist on any remote.
- The remote-newer round resolved to `v3-remote` on both sides — the
  services hold modification times well enough for newer-wins in both
  directions (ownCloud mtime extension on WebDAV, setstat on SFTP).

## Session-name budget

The short link is live on macOS (`~/Library/Caches/blitzkrieg/p0` →
profile dir), so the projection is constant across install depths. The
state files bisync actually wrote, longest name per backend including the
`.path1.lst` suffix, against the 241-byte budget:

- s3: `Users_johannes_Library_Caches_blitzkrieg_p0..bkraw_bk-saves_profiles_P04.path1.lst` — 82 bytes
- webdav: `…_p0..bkraw_profiles_P04.path1.lst` — 73 bytes
- sftp: `…_p0..bkraw_bk-root_profiles_P04.path1.lst` — 81 bytes

Each new backend brought a new remote-name shape (bucket, empty root,
path); none moved the session name more than a few bytes, because the
alias contributes its constant short form.

## Tested versus inferred

**Tested: exactly these three configurations** — MinIO over the s3
backend, `rclone serve webdav` (ownCloud vendor) over the webdav backend,
`rclone serve sftp` over the sftp backend, all on loopback. Three passing
proves three. There is no supported-provider count to quote: eleven of
rclone's 69 backends are filtered as non-candidates, and every remaining
candidate's writable behaviour is established per configuration by the
connection test, not by membership in a list. Everything not named above —
including real AWS/Wasabi/B2 endpoints, real ownCloud/Nextcloud servers,
and OpenSSH — is untested here.

## Findings

- **Typing into the `provider` field scrambles.** Each keystroke fires
  `UI_NOTIFY_EDIT_BOX_TEXT_CHANGED`, and a provider edit re-derives the
  form (by design), which rewrites the edit box and resets its cursor —
  so "Minio" typed as text landed as "oinM", and the misfiled provider
  reshaped the form under the remaining scheduled clicks. A player typing
  at normal speed hits the same per-keystroke rebuild. The example-cycle
  button is unaffected (one value per press) and is how the evidence
  selects vendors; a real fix would defer the rebuild until the edit box
  loses focus. Recorded, not fixed — the dialog is outside this packet's
  allowlist.
- **Switching backends leaves the pairing gate closed with no key.** After
  the webdav save, the next startup sync refused with
  `FingerprintChanged` — exactly the P02-M01 rule ("a changed fingerprint
  is a new pairing needing confirmation"). But no UI owns that
  confirmation: the facade's pair fallback fires only on `NotPaired`, the
  menu indicator maps the failure to the generic text, and the Cloud tab
  offers only Credentials and Backups. The evidence confirmed each new
  pairing the only way there is — deleting the machine-local record
  `cloudsync/state/P04.json` — after which the designed
  `NotPaired → pair` bootstrap ran cleanly. **Fixed in 129dcc166**: the
  credentials save retires pairing records naming another fingerprint,
  and the next sync takes the NotPaired → pair bootstrap.
- **The `text=` colon note in the P02 manifest is stale.** The schedule
  parser splits entries at their *first* colon only, so URLs type fine;
  commas remain the real separator constraint. The P02 evidence's
  seeded-document substitution is unnecessary for URLs.
- The startup-sync gate reads the raw profile config before the option
  system exists, so the harness toggled `Cloud.Sync.OnStartup` between
  rounds via `set=` inside a running session (persisting on exit) — the
  same store the settings screen writes.

## Commands (abbreviated)

The chooser walk (msg=10020 ×N) is historical: since the Provider row the
backend is chosen on the Cloud tab (msg=10011, then key=RIGHT per step) and
the dialog opens on it - see provider-row.md.

```
# services
minio server <data> --address 127.0.0.1:19100          # + MINIO_ROOT_USER/PASSWORD
rclone serve webdav <data> --addr 127.0.0.1:19200 --user dav --pass …
rclone serve sftp   <data> --addr 127.0.0.1:19300 --user sftp --pass …

# configure through the form (s3 example; webdav/sftp analogous)
BK_AUTO_UI="40:var=notransition=1,48:cmd=0x100e0104,
  56..140: msg=10020 ×43 (chooser to s3), 160..218: msg=4001 ×30 (provider→Minio),
  click/text for access_key_id, secret_access_key, region, endpoint,
  msg=4101 ×5 (scroll), click/text remote root, msg=10021 (Test),
  shot, msg=10002 (OK), set=Cloud.Sync.OnStartup=ON, exit" ./Game -windowed -profile=P04

# each sync step is one launch; verification via the independent client
rclone --config rc.conf lsl minio:bk-saves --max-depth 4   # and dav:, sftp:bk-root
```

## Human approval

**Pending.** Everything above is machine-verified through the shipped
game; the packet requires explicit human approval, to be recorded here
when given.

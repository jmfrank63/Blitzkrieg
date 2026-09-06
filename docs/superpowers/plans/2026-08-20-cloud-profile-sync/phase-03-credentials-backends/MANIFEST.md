# Phase 03 — Credentials and Backends

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Store connection details where the option system cannot corrupt them, and make the two credential-based backends work end to end.

| Packet | Depends on | Owns |
|---|---|---|
| P03-M01 | P02-M05 | credentials file, remote parameters, and their exports |
| P03-M02 | M01 | S3-compatible backend |
| P03-M03 | M02 | WebDAV backend |
| P03-M04 | M03 | connection test with classified failures |

Exit: the phase-02 cycle passes against a real S3-compatible remote and a real WebDAV server, with credentials reachable from C++ and absent from every log.

P03-M01 Windows checkpoint: `zig build test-cloudsync-creds -Dtest-mode=run`
passes 8/8 natively; `test-cloudsync-abi` 10/10 plus the consumer's
credential contract from C++, offline and twice consecutively with the live
rclone (fixtures are now seeded and cleaned — the first draft reused
`rand()`'s unseeded sequence and the second run inherited the first's
leftovers). Cross-targets compile; every other suite unaffected. Commit
`4a1159b24`.

Carried forward from P03-M01:

- **The alias indirection is the bucket answer.** The raw backend remote is
  `bkraw` (its rc parameters from `creds.remoteParams`, bucket deliberately
  absent); the sync always runs over the `bkremote` alias whose target is
  `bkraw:<bucket>` (S3) or `bkraw:` (WebDAV) from `creds.aliasTarget`.
  Path2's session-name contribution stays `bkremote:profiles/<name>`
  regardless of endpoint or bucket length, and every engine/worker test
  already uses that name. P03-M02/M03 own creating both remotes via
  `config/create`.
- The credentials path is `profiles/cloud.credentials` **relative to the
  working directory**, matching the engine's own relative-profile
  convention; the C++ consumer chdirs into its fixture for exactly that
  reason. The facade must not chdir before touching credentials.
- `Discovery.refresh` copies the search struct under the lock, and
  `Discovery.setExplicit` never frees a superseded override — an in-flight
  probe may still be reading it, and a player changes the path a handful of
  times per session. A bounded leak bought the absence of a use-after-free;
  the e2e test frees the superseded copy by hand to keep the leak checker
  honest.
- `creds.fingerprint` (`s3:<endpoint>/<bucket>` / `webdav:<url>`) is the
  `remote_fingerprint` the engine's pairing record wants; no secret
  material. `creds.parse` is `load` without the file, which is what
  `creds_save` uses on incoming ABI documents.
- Omission-preserves is implemented at the merge (`mergeOmittedSecret`), so
  an empty *and* an absent incoming secret both preserve; only
  `creds_clear_secret` blanks. After a clear there is nothing to resurrect —
  pinned in `clear_secret_removes_it`.

P03-M02 Windows checkpoint: `zig build test-cloudsync-backend -Dtest-mode=run`
passes 1/1 with a live MinIO plus the pinned rclone (`BK_TEST_MINIO` +
`BK_TEST_RCLONE`, ~5 s), and passes silently without them. Cross-targets
compile; all other suites unaffected; no orphan process. Commit `7b04a5884`.

Carried forward from P03-M02:

- **Provider coverage**: MinIO is measured. AWS, Cloudflare R2, Backblaze B2
  and Wasabi are inferred through rclone's `provider` handling and remain
  unproven by this suite — anyone claiming broader coverage owes a run
  against the real service.
- Pairing against a bucket that does not exist works: rclone's s3 `Mkdir`
  creates the bucket for the fs root the alias names, so `engine.pair`'s
  existing `operations/mkdir` covers the brand-new-account state with no
  extra code.
- The remote is unreadable as a directory, so backend assertions pull
  through `operations/copyfile` into a scratch dir and stat through
  `operations/stat` — the helpers in `backend_test.zig` are the pattern
  P03-M03/M04 should reuse. `remoteWrite` stages locally and copies up,
  which also preserves the staged file's mtime for newer-wins scenarios.
- MinIO's API accepts TCP a moment before it is actually serviceable; the
  fixture waits for the port and then one extra second. A `MinioSpawnFailed`
  on a machine with the env var set is a real failure, not a skip.
- `test-cloudsync-backend` is a new build step (build.zig amended, same
  shape as the others).

P03-M03 Windows checkpoint: `zig build test-cloudsync-backend -Dtest-mode=run`
passes 2/2 with both backends live (~22 s), skip path clean, cross-targets
compile, no orphans. Commit `dc099ecb6`.

Carried forward from P03-M03:

- **The modify-window finding.** WebDAV holds mtimes at one-second
  granularity, and bisync compares listings under a one-second modify
  window: an overwrite of a remote file landing within one second of that
  file's previous listing entry reads as a *size-only* change, the winner
  comparison then has a zero time for that side, and bisync falls back to
  renaming both copies (`.conflict1`/`.conflict2` — nothing lost, nobody
  ranked). Measured against `rclone serve webdav`. Field consequence: two
  machines saving the same file within one second of each other get the
  rename-both fallback rather than newer-wins. Safe, but P07's conflict UI
  should not promise "newest always wins" unconditionally.
- The client must send `vendor: "owncloud"` (or another mtime-capable
  vendor) for WebDAV: a plain vendor drops modification times entirely and
  would misresolve every newer-wins comparison. `rclone serve webdav`
  implements the ownCloud mtime extension; a forged 2026-01-15 mtime
  round-tripped byte-identical.
- `config/create` is always called with `opt.obscure = true`: rclone
  obscures password-typed fields itself (webdav `pass`), and non-password
  fields (S3's secret) pass through untouched — one call shape for every
  backend.
- bisync inside the daemon resolves the alias for its session name: the
  listings are named `..bkraw_profiles_hero`, not `..bkremote_...`. The
  budget arithmetic still holds (`bkraw` is as short as `bkremote`), but
  anyone renaming the raw remote must re-check the session-name budget.
- A spawned test child needs a real environment map — with an empty one a
  Windows process dies before `main` (no `SystemRoot`) and reads as a
  server that never comes up.

P03-M04 Windows checkpoint: engine 16/16 natively (17 with the live server —
all four probe fixtures classified against a real WebDAV), abi 10/10 plus
the consumer offline and live including the dead-endpoint probe classified
from C++, worker and backend suites green both ways, cross-targets compile,
no orphans. Commit `17d521aa5`.

Carried forward from P03-M04:

- **The worker now applies `cloud.credentials` to the daemon** before the
  first job: `bkraw` plus the `bkremote` alias via `config/create` with
  `opt.obscure`, from `<game_dir>/profiles/cloud.credentials`. No file →
  skip, and whatever `rclone.conf` already holds is used (the fixture
  path). This closed the last gap between the ABI and a real cloud: save
  credentials, probe, pair, sync — all through the exports.
- The probe sends `_config: {Retries: 1, LowLevelRetries: 1,
  ConnectTimeout: "3s", Timeout: "5s"}` — without it, S3's internal retry
  schedule turns a dead endpoint into `timed_out` (measured). A probe means
  one bounded attempt.
- A missing credential ("secret_access_key not found", captured live via a
  cleared secret) classifies as `auth_failed`; `remote_missing` is the new
  outcome for "server answered, root absent", recovering to the credentials
  dialog — a fresh account's not-yet-created bucket is the same shape and
  the dialog's copy should soften it.
- `worker.State.testing` (6) and `worker.Outcome.connection_ok` (4) are
  appended, never inserted; the ABI asserts pin them. The probe handle's
  failure text begins with the classified outcome's tag
  (`remote_unreachable: dial tcp …`) — P07 parses the prefix, or asks for a
  typed export then.
- `bk_cloudsync_test_connection` takes `game_dir` — the packet's
  no-argument form had no way to find the credentials or spawn the daemon
  before any `begin`.
- `worker.zig` was amended beyond the packet's allowlist (the job kind and
  credential application live there); same pragmatism as build.zig
  amendments, recorded here.

**Phase 03 exit: met on Windows.** The phase-02 cycle passes against a real
S3-compatible remote (MinIO) and a real WebDAV server, credentials are
reachable from C++ — saved, probed, and driving the sync through the exports
— and no secret appears in any output: `creds_load` withholds it, error
texts are redacted, and the live tests assert its absence.

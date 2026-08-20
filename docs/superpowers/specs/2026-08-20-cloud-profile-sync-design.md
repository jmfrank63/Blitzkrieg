# Cloud Profile Sync

Player profiles live in `<game>/profiles/<name>/` (saves, screenshots, config.cfg)
— see `Sources/src/StreamIO/ProfilePaths.h`. This document describes syncing that
directory to a cloud provider so a player's campaign follows them between
machines.

## Decision

**rclone is the engine. Its rc API is the interface. The game speaks that API
over a localhost socket to an `rclone rcd` child process.**

The appeal of rclone was never the 70-odd backends by themselves — it is that
they sit behind one interface. That interface already exists and is stable: the
[rc API](https://rclone.org/rc/), a JSON-in/JSON-out RPC surface. It has exactly
two transports, and they take identical payloads:

| | `librclone` (in-process) | `rclone rcd` (child process) |
|---|---|---|
| Build dependency | Go + cgo, for all six target triples | none |
| Prebuilt artifacts | **none published** (checked v1.75.0: 51 release assets, zero librclone) | official signed binary per platform |
| Ship size | ~50-80 MB `.dll`/`.dylib`/`.so` | 84 MB exe (macOS arm64, v1.75.0) |
| Coupling | Go runtime, GC threads and signal handlers inside the game process; upstream says it cannot be unloaded on Windows | separate process, crash-isolated, killable |
| Transport | direct C call, 4 symbols | HTTP to 127.0.0.1 |
| Blast radius of a bug | takes the game down | orphaned process to reap |

Both cost the same download. The child process wins on everything else, so that
is what we build.

This is not a fork in the road we have to get right the first time. The rc
payloads are identical across both transports, so `rc.zig` — the client that
builds the JSON and interprets the replies — is written once and is the whole
investment. Swapping to in-process `librclone` later means replacing one
`send()` function, not the design.

### Why not add-only sync

Two `sync/copy` calls (local to cloud, cloud to local) with `UpdateOlder` and
`BackupDir` is a genuinely tempting alternative: `copy` never deletes on the
destination, so it needs none of bisync's state — no workdir, no session name
and therefore no `NAME_MAX` exposure, no sentinel, no resync, no delete
ratios. It was tested and it works, conflicts included: the newer side wins
and the loser lands in the backup directory.

It is rejected because it cannot express a deletion. A save deleted on one
machine is restored from the cloud on the next pull (verified), so stale
autosaves become permanent and the profile only ever grows. Deletion has to
propagate; the requirement is that it be *recoverable*, not that it be
impossible.

### Why not link librclone now

It is the tempting answer and it is a trap at this point in the project:

- rclone publishes **no** prebuilt librclone artifacts. We would build it
  ourselves, with cgo, for `x86_64-windows-msvc`, `x86_64-windows-gnu`,
  `x86_64-linux-gnu`, `aarch64-linux-gnu`, `x86_64-macos` and `aarch64-macos`.
  Cross-compiling cgo across operating systems is the hard case, not the easy
  one, and this repo currently cannot even cross-build the MSVC C++ from macOS.
- Upstream's own README recommends the shared library over the static archive
  ("linking with static library requires most compatibility and is less likely
  to work"), so the static-link-it-all-in dream is off the table regardless.
- A Go runtime inside a 2003-era C++ game process brings its own scheduler,
  GC threads and signal handlers, next to SDL, macOS Spaces fullscreen
  transitions and `ModernAssert.h`'s crash-handler seam. That is a debugging
  surface we do not need to buy.

If we ever want it: the C ABI is four symbols (`RcloneInitialize`,
`RcloneFinalize`, `RcloneRPC(char*, char*) -> {char* Output; int Status;}`,
`RcloneFreeString`), trivially declared in Zig and resolvable with
`std.DynLib`, so it can even be an optional `dlopen` rather than a link-time
dependency.

## Verified behaviour

Everything below was exercised against rclone v1.75.0 on macOS arm64 before this
document was written, not taken from the manual.

- `rclone rcd --rc-addr 127.0.0.1:<port> --rc-user … --rc-pass …` starts in
  under 3 s and idles at ~55 MB RSS.
- `sync/bisync` is a first-class rc method. A `resync:true` call bootstraps the
  pairing; subsequent calls propagate both directions.
- Conflicts are handled the way we want, for free. With
  `conflictResolve:"newer"`, a file edited on both sides resolves to the newer
  content on *both* sides and the loser is preserved as
  `mission1.sav.conflict1` on *both* sides. Nothing is silently destroyed, and
  the `.conflict1` suffix keeps the loser out of the game's `*.sav` load list
  while remaining recoverable by hand.
- `"_async": true` returns `{jobid, executeId}` immediately; `job/status`
  polls it to `{finished, success, error, output}`. The game never blocks on a
  socket — it polls a job id from the main loop.
- Remotes can be declared inline as JSON objects (`{"type":"s3",
  "provider":"Cloudflare", …}`) or created with `config/create`, so no
  `rclone.conf` on disk is required and credentials stay in our own config.

## Architecture

```
Sources/src/CloudSync/
  cloudsync.zig     root module + C ABI exports (mirrors StreamIOZig's pattern)
  rc.zig            rc client: build JSON, POST, parse reply, poll job/status
  daemon.zig        spawn//health-check/reap the rcd child, port + token
  plan.zig          profile dir -> bisync params; short-link management;
                    filter rules; sentinel; state paths
  CloudSync.def     export list for the Windows build
```

Zig, not C++: `StreamIOZig` and `Blitz64` already establish the
Zig-dylib-exported-to-C++ pattern in `build.zig`, and Zig 0.16's std gives us
HTTP, JSON and TLS with no third-party dependency. (Confirmed: a real HTTPS
fetch plus HMAC-SHA256 out of a 1.1 MB static exe, wiring into the
`std.Io.Threaded` service this project already has in
`Sources/src/StreamIOZig/io_service.zig`.)

The C++ side gets a small facade next to `NProfile`, exposing roughly:

```
bool  CloudSyncAvailable();                  // rclone binary found and runnable
int   CloudSyncBegin( const char *profile ); // -> handle, non-blocking
int   CloudSyncPoll( int handle );           // idle | running | done | failed
const char *CloudSyncError( int handle );
```

Process spawning already has a cross-platform home in
`Sources/src/Platform/System.cpp` (`CreateProcessA` / `posix_spawn`); the daemon
launcher should use it rather than growing a second one.

## Sync semantics

We do not write a diff engine. `sync/bisync` is the diff engine, and it has had
far more adversarial testing than anything we would write.

- **Pairing.** First sync for a profile is `resync:true`, which establishes the
  baseline listings. Every later sync omits it. If bisync ever aborts with
  "must run --resync to recover", that is a state we surface to the player as
  a one-click repair, never something we trigger silently — a resync after
  real divergence can overwrite one side.
- **A resync must carry `resyncMode: "newer"`.** `conflictResolve` does not
  apply during a resync; resync defaults to Path1 winning, and it renames
  nothing. Verified, and it is the sharpest edge in this design: pairing a
  machine whose local save is *older* than the cloud copy silently destroyed
  the cloud version — no `.conflictN`, no trash, nothing to recover from. With
  `resyncMode: "newer"` the newer copy survives on both sides. Pairing is
  exactly the moment both sides are most likely to hold different content, so
  this is not an edge case.

  A resync **does** honour `backupDir1`/`backupDir2` (verified in both
  directions), so the overwritten version is recoverable from the losing
  side's run-scoped trash. The original failure destroyed the save only
  because no backup directory was passed at all — which is why pairing carries
  them and asserts the loser landed there.
- **Conflicts.** `conflictResolve:"newer"`, losers kept as `.conflictN`. The
  rule we commit to: **a sync never destroys a save.**
- **Deletes propagate in both directions, but nothing is unlinked.** A delete
  has to reach the other machine — otherwise deleting forty stale autosaves is
  impossible, because every surviving copy resurrects them on the next run.
  bisync does this symmetrically (verified: removing a file on the cloud side
  removes it locally). What we add is that it is never destructive:
  `backupDir1` and `backupDir2` divert every deletion and overwrite into a
  trash directory, relative paths preserved (`trash/saves/m2.sav`).

  **There are two trashes, not one, and a fresh directory per run.** rclone
  requires each backup directory to live on its own side's filesystem —
  pointing `backupDir2` at a local path while Path2 is a remote fails the run
  outright with `parameter to --backup-dir has to be on the same remote as
  destination` (verified). So `backupDir1` is local under the profile and
  `backupDir2` is on the remote.

  Each run gets its own trash directory, `<trash>/<run_id>/`. rclone
  **overwrites** an existing backup at the same resulting path: deleting
  `saves/m2.sav`, recreating it and deleting it again left only the second
  version and destroyed the first (verified). Save filenames recur constantly
  — `quick.sav` and the autosaves are the same name every time — so a shared
  trash root loses recovery copies during ordinary play. A unique per-run
  `_config.Suffix` is an equally valid fix and also verified, but only when
  the suffix is itself unique per run; a constant one collapses identically.

  Between `.conflictN` and the two run-scoped trashes, both ways a save can be
  at risk are recoverable. Trash is pruned whole runs at a time, not by sync.
- **One delete always passes; mass deletion trips the breaker.** `maxDelete`
  is a *percentage* (`deleted / oldCount > maxDelete/100` aborts), an awkward
  shape for a profile holding three saves — deleting the only save would be
  100%. The `.bkprofile` sentinel fixes this as a side effect: it counts
  toward `oldCount`, so a single delete is at worst 1-of-2 = 50%, and the
  comparison is `<=`. At `maxDelete: 50` that is exactly the policy we want,
  with no dynamic ratio computation:

  | profile | deleted | result |
  |---|---|---|
  | sentinel + 1 save | 1 | passes (50%) |
  | no sentinel + 1 save | 1 | aborts |
  | sentinel + 4 saves | 1 | passes |
  | sentinel + 4 saves | 3 | `too many deletes` |

  A `too many deletes` abort is then a genuine mass-delete event, surfaced to
  the player as a prompt — "the cloud copy looks emptied, mirror that?" —
  rather than as a failure. Pin this with a test: **sentinel plus one save,
  delete it, sync must succeed.** That test is what stops someone removing the
  sentinel as cosmetic.
- **Scope.** Path1 is `profiles/<name>/`, path2 is `<remote>/profiles/<name>/`.
  Profiles sync independently, so a corrupt pairing on one does not touch
  another. **Nothing that is not a profile lives under path2** — the trash and
  the config backups are siblings of `profiles/`, because anything beneath the
  synced prefix is by definition synced back down to every machine:

  ```
  <remote>/profiles/<name>/               path2, synced
  <remote>/trash/<name>/<run_id>/          backupDir2
  <remote>/config-backups/<name>/<host>/   config snapshots
  ```
- **The remote directories must exist before the first pairing.** bisync needs
  both base directories present and otherwise aborts a first resync with
  `error reading source root directory: directory not found` (verified, even
  against a local path2). An `operations/mkdir` precedes pairing — the empty
  remote is the normal first-run state, not an edge case.
- **Filters.** Excluded from sync:
  - `config.cfg` — carries `GFX.Mode`, `GFX.Monitor`, `GFX.FullScreen`. This
    codebase spent real work making display choices persist *per machine*;
    pushing a 4K desktop's monitor layout onto a 14" MacBook would undo that.
    It is backed up rather than synced — see Config backups below.
  - `screenshots/` — large, low value. Opt-in later.
  - `*.tmp-rename` — `NProfile::Rename`'s intermediate state.
- **Clock skew** is what `conflictResolve:"newer"` runs on. Consider
  `checkAccess` and a size/checksum comparison as hardening once the basic path
  works.

## Config backups

`config.cfg` is never synced, but it is worth having a copy off the machine.
Backups are a separate, one-way mechanism with no bisync involvement:

- After a successful sync, the current config is snapshotted to
  `<remote>/config-backups/<name>/<host>/<timestamp>.cfg` — a sibling of
  `profiles/`, not a child of it — with a single `operations/copyfile`. No listings, no session state, no delete
  ratios — none of bisync's machinery applies.
- **Backups are never pulled automatically.** Restoring is always an explicit
  player action, which is the whole point of the split: the cloud holds the
  history, the local machine keeps authority over its own display settings.
- Snapshots are keyed by host, so machines never overwrite each other's
  history and "restore the desktop's settings onto the laptop" is a
  meaningful request. Retention is the last N per host, pruned with
  `operations/list` + `operations/deletefile`.
- **Restore defaults to a merge, not a copy**: every key except `GFX.*` is
  taken from the backup, and the local display settings are kept. A full
  restore is offered but warned about. It is survivable — a resolution absent
  from the local SDL mode list falls back to Auto, and a disconnected monitor
  falls back to display 0 — but "survivable" is not "wanted", and the failure
  is silent.
- The current config is copied into the local trash before a restore
  overwrites it, so restoring is itself undoable. Applying is idempotent: a
  retry after an interrupted apply reuses the existing snapshot rather than
  taking a new one.
- **A restore is staged, never applied live.** The game rewrites `config.cfg`
  from its in-memory options at shutdown (`GameMain.cpp:1182`) and from eight
  other `SerializeConfig( false, ... )` call sites across the interface code,
  so a restored file written under a running game is discarded before the
  player ever sees it. The download goes to a nonce-scoped staged directory
  `profiles/<name>/.cloudsync-restore/<nonce>/` holding the payload, a
  metadata file naming the mode and the payload hash, and a `COMMIT` marker
  written last; a stage without `COMMIT`, or failing its hash, is deleted
  rather than guessed at. Nonce scoping means the `COMMIT` write is the single
  publication point, so a failed download never destroys a previously staged
  restore and two requests cannot interleave in one directory. Undo takes the same path, and acceptance tests a restore across a real
  restart, because a same-session check passes while the feature is broken.
- **The merge runs at apply time, in Zig, behind one export.** At the next
  startup — before the option system reads the config — the game calls a
  local-only `apply_pending_restore`, which snapshots the current `config.cfg`
  for undo, merges the stage against the file *as it then stands*, and
  installs the result. Doing the merge then rather than at stage time means
  local `GFX.*` values changed during the intervening session are respected.
  The snapshot is taken there for the same reason: a copy made when the
  restore was requested would undo to a file hours out of date. It is named
  after the stage nonce and written only once, because a crash between
  installing the new `config.cfg` and clearing the stage makes the next launch
  apply the same stage again — and an unkeyed snapshot would then capture the
  already-restored file, leaving undo recovering the restore instead of the
  original. The operation
  needs no daemon and no credentials, so a restore already downloaded still
  completes if the feature is later switched off.
- Only local `GFX.*` values are preserved across that window. Every other key
  comes from the backup by design, so a sound or gameplay setting changed
  after requesting a restore is replaced when it applies — worth stating in
  the UI rather than implying that nothing is lost.

## In-game configuration

The provider is chosen in the existing settings screen, as a fifth tab
alongside GFX, GamePlay, Multiplayer and Sound.

Almost none of this needs code. The tab bar is data-driven: a division is just
the first dot-separated component of an option name
(`COptionSystem::GetDesc`, `OptionSystemInternal.cpp:168`), and
`CInterfaceOptionsSettings::Create` groups options by division in
first-encounter order, lighting up list `1000+n` and button `10007+n` for
each. Registering `Cloud.*` options therefore creates the tab. Options are
declared in `Data/Configs/defconf.cfg` (`EditorType`, `Flags`, `Order`,
`Type`, `Action`, `Default`, `KeyName`), and labels come from
`Data/Textes/Options/Cloud.name.txt` plus a `.name`/`.tooltip` pair per
option.

`Data/UI/OptionsSettings.xml` already defines six tab buttons (10007-10012)
and six lists (1000-1005) against today's four divisions, so **the fifth tab
needs no XML change**. Two things to repair while adding it:
`_E_BUTTON_CHANGE_DIVISION_END = 10009` and `_E_LIST_END = 1002` are stale —
they claim three divisions, and grep finds no use of either constant — and
`Create()` indexes `_BEGIN + nMaxDivision` with no bounds check, so a seventh
division would `checked_cast` a null child. Correct the constants and add the
guard.

### Credentials do not go in the option system

`COptionSystem::Set` truncates every string option longer than 12 characters
down to 8, unless the option's action happens to be `SetVideoMode`
(`OptionSystemInternal.cpp:186-192`):

```cpp
const bool bCanUseLongString = (pOpt != 0) && (pOpt->szAction == "SetVideoMode");
if ( (szStr.size() > 12) && !bCanUseLongString )
    szStr.resize( 8 );
```

A 20-character access key, a 40-character secret and any endpoint URL are all
silently destroyed by that. So the Cloud tab carries only the values the
options list can safely represent — toggles and droplists:

```
Cloud.Enabled          EditorType 3   Off / On
Cloud.Provider         EditorType 3   Off / S3 / WebDAV
Cloud.Sync.OnStartup   EditorType 3   Off / On
Cloud.Sync.OnSave      EditorType 3   Off / On
Cloud.Sync.OnExit      EditorType 3   Off / On
Cloud.Config.Backup    EditorType 3   Off / On
```

Connection details are edited in a dedicated dialog reached from the tab — the
player-profile dialog is the precedent for an edit-box screen — and written to
`profiles/cloud.credentials`, never through the option system.

That file uses a **tagged schema**, because the two backends share almost no
fields: an `s3` arm carrying vendor, endpoint, bucket, region, access key and
secret, and a `webdav` arm carrying URL, vendor, user and password. The dialog
shows one arm at a time. Note that rclone's S3 `provider` names the vendor
behind the protocol (AWS, Cloudflare, Minio) while the game's `Cloud.Provider`
selects the protocol itself — same word, different question, so the stored
field is `s3_provider`.

Discovery of the rclone binary is cached so that availability and the detailed
status can never disagree, and saving credentials invalidates that cache —
`rclone_path` lives in this file, so a player who points the game at a working
binary must see it recognised immediately rather than after a restart.

The secret is never handed back out: the load path returns a `has_secret` flag
and the dialog shows a placeholder. Saving without touching that field
therefore has to **preserve** the stored secret rather than write an empty
one, or editing an endpoint would silently destroy the credential. Clearing is
a separate deliberate action.

That separation is not only about the truncation. `config.cfg` is uploaded by
the backup mechanism above, so **credentials kept in it would be pushed into
the cloud service they unlock**. `cloud.credentials` is excluded from both
sync and backup, and must stay that way.

## Lifecycle

- **Startup:** if cloud sync is enabled, start the daemon and issue an async
  bisync *before* the profile config is read. The main menu shows a "syncing"
  state; the player can skip and play offline, which marks the profile dirty
  for the next attempt.
- **In game:** nothing. No syncing mid-mission.
- **After a save completes and on exit:** async bisync, polled from the main
  loop. On exit we wait with a bounded timeout and a visible indicator rather
  than blocking indefinitely.
- **Daemon lifetime:** spawn on demand, kill on exit, bind to 127.0.0.1 only,
  random port, random per-launch `--rc-user`/`--rc-pass`. On startup, reap any
  orphan from a previous crashed run before spawning.

## Where settings live

Three files, split by who owns the value and who is allowed to see it:

| file | holds | synced? | backed up? |
|---|---|---|---|
| `config.cfg` (per profile) | the `Cloud.*` options from the settings tab, alongside everything else | no | yes |
| `profiles/cloud.credentials` | endpoint, bucket, access key, secret | **no** | **no** |
| `<remote>/.../config-backups/` | config snapshots per host | n/a | n/a |

Credentials are plaintext on disk in the first cut — the same posture as the
rest of the profile — and that must be stated plainly in the UI, not just
here. Platform keychain / DPAPI storage is a follow-up. What is *not*
negotiable even in the first cut is the file split: `config.cfg` leaves the
machine, so credentials cannot live in it.

`Cloud.Rclone.Path`, an optional explicit path to the rclone binary, sits in
`cloud.credentials` too — it is machine-specific, so it has no business in a
file that gets restored onto a different machine.

## Gotchas found the hard way

- **`NAME_MAX` on the bisync session name.** bisync derives its state
  filenames by mangling *both* canonical path strings into one name
  (`tmp_bksync_local..tmp_bksync_remote.path1.lst`), and past 255 bytes every
  run aborts with `file name too long`. The budget is exact and computable:
  `NAME_MAX (255) - 14` for the longest suffix (`.path1.lst-new`; `-old` and
  `-err` are the same length, `.lck` is shorter), so **241 bytes** for
  `mangle(path1) + ".." + mangle(path2)`.

  Only the local side pays full price. `bilib.FsPath` branches on
  `if name == "local"`, using the raw absolute path for local and a bare
  `remotename:root` for everything else, so a named cloud remote costs ~25
  bytes. Alias remotes do **not** help — the alias backend resolves through to
  the wrapped Fs, which arrives as `local` with the target's absolute path.
  Relative paths do not help either; the local backend absolutizes them.

  Measured: a Steam-style install
  (`.../steamapps/common/Blitzkrieg/profiles/Panzerkommandant`) against a
  short cloud remote produces a 212-byte session name. It fits, but 29 bytes
  of headroom is not a margin worth shipping on.

  **The fix is to never hand bisync the install path.** A short, stable,
  game-managed link is used as Path1 instead. rclone does not resolve a
  symlinked root, so the short name survives into the session name:
  pointing bisync at `/tmp/bkp` -> a 199-byte profile directory produced the
  session `tmp_bkp..tmp_bkremote` (21 bytes), and the profile — including the
  leading-space save filename — synced through it correctly. On Windows this
  is a **junction** (`mklink /J`), which unlike a symlink needs no
  administrator rights or Developer Mode; that path still needs verifying on
  the Windows machine. `plan.zig` creates the link under a fixed short root
  (`%LOCALAPPDATA%\bk\` / `~/.cache/blitzkrieg/`), repoints it when the
  active profile changes, and still validates the projected session length as
  a backstop before every run.
- **The all-files-changed safety abort will fire during normal play.**
  `bisyncops.go` aborts with `all files were changed` whenever a side has no
  unchanged file at all (`deltaSet.foundSame`, "true if found at least one
  unchanged file"). It exists to catch DST-shifted timestamps, but a profile
  holds a handful of saves — overwrite the only autosave and 100% of the side
  has changed. This reproduced immediately in testing on a one-file profile.

  `force: true` silences it, but it also disables the excess-deletes guard
  (`ds.excessDeletes()`, the check that stops a wiped cloud side from
  emptying the local one), so it is the wrong lever. Instead the profile
  carries a **sentinel file** — `.bkprofile`, written once at pairing and
  never rewritten. It guarantees `foundSame` on both sides while leaving
  `force` off and the delete guard armed. Verified: rewriting *every* save on
  both sides then syncing succeeds with the sentinel present, resolves the
  conflict, and preserves the loser as `.conflict1`. It then does double duty
  — being counted in `oldCount` is what keeps a single deletion under the
  `maxDelete` ratio (see Sync semantics). Two separate guards rest on this one
  file.

  **It is written on Path1 only and never seeded on the remote.** Creating it
  independently on both sides gives the copies different modification times,
  and bisync rejects that during resync — `Modtime not equal in listing ...
  .bkprofile`, then `path1 and path2 are out of sync, run --resync to recover`
  (verified). A second machine that has never paired must check whether the
  remote already carries a sentinel and let the resync deliver it rather than
  writing its own.
- **`maxDelete` defaults to 0 over the rc API — the CLI's 50 does not apply.**
  `cmd.go` applies `DefaultMaxDelete = 50` on the command-line path; `rc.go`
  builds a zero-valued `Options{}` and only assigns `opt.MaxDelete` if the
  caller passed `maxDelete`. The zero value means *any* delete exceeds the
  ratio. Verified: deleting one file out of five aborted the run with `too
  many deletes`, and the same call with `maxDelete: 50` succeeded. Worse,
  `maxDelete: 0` is not a way to get add-only behaviour — it aborts the entire
  run, so a sync carrying one deletion and one new file delivered neither.
  Always send `maxDelete` explicitly.
- **rc errors are terse.** The RPC reply is `{"error": "bisync aborted",
  "status": 500}` and nothing more; the actionable detail is only in the log.
  Always run the daemon with `--log-file` and surface its tail on failure. In
  async mode `job/status` returns the run's log in `output.output`, which is
  the better source.
- **Do not shell out to `rclone bisync` per operation.** One long-lived daemon,
  many RPCs. Per-call process spawn costs the ~55 MB and the startup latency
  every time.
- **The leading-space save filename** (`profiles/<name>/saves/ Mission Start
  Auto.sav`) is real and must survive a round trip. Put it in the test corpus.

## Shipping

rclone is MIT, so redistribution is fine, but 84 MB per platform is not going
into the base install. Cloud sync is an optional feature: if the binary is
absent, `CloudSyncAvailable()` is false and the UI offers a download or a path
picker. A player who already has rclone installed points at their copy.

## Implementation plan

The packetised plan lives at
`docs/superpowers/plans/2026-08-20-cloud-profile-sync/` — 34 packets across
nine phases, starting at `phase-00-rc-transport/P00-M01-rc-json-client.md`.
The milestones below are the shape of it; the plan is the authority on
sequencing and gates.

## Milestones

1. `rc.zig` against a hand-started `rclone rcd` — `core/version`, `sync/bisync`,
   `job/status`. No game integration.
2. `daemon.zig` — spawn, health check, auth, reap, orphan cleanup.
3. `plan.zig` — profile dir to bisync params, filters, the short-link
   (symlink/junction) management, the `.bkprofile` sentinel, and session-name
   length validation as a backstop. Tests over a fake profile tree including
   the leading-space save and an install path deep enough to blow the budget
   without the link.
4. C ABI + C++ facade; wire the startup and post-save hooks; "syncing" UI state.
5. Real backends end to end (S3-compatible first — R2/B2/MinIO — then WebDAV).
6. Settings integration — `Cloud.*` options in `defconf.cfg`, the text
   entries, the credentials dialog, and the stale-constant/bounds-check repair
   in `InterfaceOptionsSettings.cpp`.
7. Config backup and restore, including the `GFX.*`-preserving merge.
8. Only if wanted: swap the transport to in-process `librclone`, changing
   nothing above the `send()` boundary.

## Open questions

- Where credentials live long term (keychain / DPAPI / libsecret);
  `profiles/cloud.credentials` is the first cut, not the destination.
- Whether screenshots sync at all, or only on explicit request.
- Whether `config.cfg` eventually gets a key-level split rather than staying
  excluded.
- Whether the rclone binary is bundled in a separate download or fetched by the
  game on first enable (and if fetched, signature verification).

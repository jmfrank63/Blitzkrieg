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
  plan.zig          profile dir -> bisync params; filter rules; state paths
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
  "must run --resync to recover", that is a state we surface to the player as a
  one-click repair, never something we trigger silently — a resync after real
  divergence can overwrite one side.
- **Conflicts.** `conflictResolve:"newer"`, losers kept as `.conflictN`. The
  rule we commit to: **a sync never destroys a save.**
- **Scope.** Path1 is `profiles/<name>/`, path2 is `<remote>/profiles/<name>/`.
  Profiles sync independently, so a corrupt pairing on one does not touch
  another.
- **Filters.** Excluded from sync:
  - `config.cfg` — carries `GFX.Mode`, `GFX.Monitor`, `GFX.FullScreen`. This
    codebase spent real work making display choices persist *per machine*;
    pushing a 4K desktop's monitor layout onto a 14" MacBook would undo that.
    Revisit as a key-level merge (gameplay/audio synced, `GFX.*` local) once
    sync itself is proven.
  - `screenshots/` — large, low value. Opt-in later.
  - `*.tmp-rename` — `NProfile::Rename`'s intermediate state.
- **Clock skew** is what `conflictResolve:"newer"` runs on. Consider
  `checkAccess` and a size/checksum comparison as hardening once the basic path
  works.

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

## Configuration

`profiles/cloud.cfg` (global, not per-profile):

```
Cloud.Enabled     = 1
Cloud.Remote.Type = s3
Cloud.Remote.Path = bk-profiles/
Cloud.Rclone.Path = <optional explicit path to the rclone binary>
```

with the backend's parameters alongside. Credentials are plaintext on disk in
the first cut — same posture as the rest of the profile — and the doc must say
so plainly. Platform keychain / DPAPI storage is a follow-up, not a blocker.

## Gotchas found the hard way

- **`NAME_MAX` on the bisync session name.** bisync derives its state filenames
  by mangling *both* canonical path strings into one name
  (`tmp_bksync_local..tmp_bksync_remote.path1.lst`). With long absolute paths
  this exceeds 255 bytes and every run aborts with `file name too long`. Alias
  remotes do **not** help — they resolve back to the canonical path before
  mangling. Mitigation: keep `workdir` under the game directory and validate
  the projected session-name length at setup, reporting a clear error instead
  of an inscrutable abort. A deep Windows install path is the realistic way a
  player hits this.
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

## Milestones

1. `rc.zig` against a hand-started `rclone rcd` — `core/version`, `sync/bisync`,
   `job/status`. No game integration.
2. `daemon.zig` — spawn, health check, auth, reap, orphan cleanup.
3. `plan.zig` — profile dir to bisync params, filters, session-name length
   validation. Tests over a fake profile tree including the leading-space save.
4. C ABI + C++ facade; wire the startup and post-save hooks; "syncing" UI state.
5. Real backends end to end (S3-compatible first — R2/B2/MinIO — then WebDAV).
6. Only if wanted: swap the transport to in-process `librclone`, changing
   nothing above the `send()` boundary.

## Open questions

- Where credentials live long term (keychain / DPAPI / libsecret).
- Whether screenshots sync at all, or only on explicit request.
- Whether `config.cfg` eventually gets a key-level split rather than staying
  excluded.
- Whether the rclone binary is bundled in a separate download or fetched by the
  game on first enable (and if fetched, signature verification).

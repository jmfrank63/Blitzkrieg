# Cloud Profile Sync Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to implement exactly one packet at a time. Do not redesign the transport or combine packets. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Sync `profiles/<name>/` to a cloud provider by driving rclone's rc API over a localhost socket, with the provider chosen from a new Cloud tab in the settings screen, and `config.cfg` backed up per host rather than synced.

**Architecture:** A Zig module `Sources/src/CloudSync` speaks rclone's JSON rc API to a child `rclone rcd` process, and exports a C ABI to C++ exactly as `StreamIOZig` does — one that grows packet by packet under the amendment rule in `EXECUTION.md`, never stubbed ahead of its behaviour. `sync/bisync` is the diff engine; the plan never writes one. Path1 is a short game-managed link to the profile directory, path2 is a named remote. Credentials live outside the option system and outside `config.cfg`.

**Tech Stack:** Zig 0.16.0 (`std.http.Client` over `std.Io.Threaded`, `std.json`, no third-party dependency), C++17 game modules, rclone v1.75.0 or newer as an optional external binary, Windows x64, Linux x64, macOS arm64/x64.

---

## Authoritative documents

Read these before executing a packet:

1. `docs/superpowers/specs/2026-08-20-cloud-profile-sync-design.md`
2. `docs/superpowers/plans/2026-08-20-cloud-profile-sync/README.md`
3. `docs/superpowers/plans/2026-08-20-cloud-profile-sync/EXECUTION.md`
4. The selected phase `MANIFEST.md`
5. The selected packet

The design document records behaviour measured against real rclone, not read from its manual. Where a packet and the design disagree, stop and report; do not silently prefer either.

## Milestone boundary

Included:

- an rc client, daemon supervisor, and rclone discovery in Zig;
- short-link management, filters, sentinel, and bisync parameter construction;
- pairing, async job polling, error classification, and trash pruning;
- a C ABI plus C++ facade, and the startup/post-save/exit hooks;
- a Cloud tab in the settings screen and a credentials dialog;
- per-host `config.cfg` backup and a `GFX.*`-preserving restore;
- S3-compatible and WebDAV backends;
- native acceptance on macOS, Windows, and Linux.

Excluded:

- linking `librclone` in-process (design records the four-symbol ABI for later);
- OAuth backends — Google Drive, Dropbox, OneDrive — which the rclone binary's own config covers;
- syncing `screenshots/`;
- a key-level merge of `config.cfg` into the sync set;
- keychain / DPAPI / libsecret credential storage;
- bundling or auto-downloading the rclone binary.

## Global invariants

- **A sync never destroys a save.** Conflict losers survive as `.conflictN`; deletions and overwrites are diverted to trash by `backupDir1`/`backupDir2`. A packet that can lose a save is wrong even if its tests pass.
- **There are two trashes, and a fresh directory per run.** `backupDir1` must live on Path1's filesystem and `backupDir2` on Path2's. A local path passed as `backupDir2` against a remote Path2 fails the run with `parameter to --backup-dir has to be on the same remote as destination`. Both are `<trash>/<run_id>/`: rclone overwrites a backup at an existing path, so a shared root destroys the earlier copy of any filename that recurs — and save filenames recur constantly.
- **Nothing but the profile lives under Path2.** The trash and the config backups are siblings of `profiles/`, never children of it; anything beneath Path2 is synced back down by definition.
- **A restored config is staged, never written live.** The game rewrites `config.cfg` from memory at shutdown and on settings OK, so a live write is discarded before the player sees it.
- **Every `resync` carries `resyncMode: "newer"`.** `conflictResolve` does not apply during a resync, which defaults to Path1 winning and renames nothing — pairing a machine holding an older save silently destroys the newer cloud copy without a conflict file or a trash entry.
- **The sentinel is written on Path1 only, never seeded on the remote.** Two independently created copies differ in modification time and bisync rejects the resync as out of sync.
- Path1 is always the short game-managed link, never the install path. The projected bisync session name is validated before every run.
- `maxDelete` is sent explicitly on every `sync/bisync` call. The rc API defaults it to 0, not to the CLI's 50.
- The `.bkprofile` sentinel is written at pairing and never rewritten. Two safety guards depend on it.
- `force: true` is never sent. It disables the excess-deletes guard.
- `config.cfg` is never in the sync set. It is backed up one-way and restored only on explicit request.
- Credentials never enter the option system, `config.cfg`, or any file that is synced or backed up.
- The game never blocks on a socket. Every rc call that can take time uses `_async` and is polled from the main loop.
- The rcd child binds 127.0.0.1 only, on a per-launch random port with a per-launch random user and password.
- Zig owns the sync logic; C++ owns only the facade, the hooks, and the UI.
- **No HTTP call ever runs on the main thread.** `_async` makes the *job* asynchronous server-side; the initiating POST and every `job/status` POST still block the caller. The engine owns a worker; `poll()` reads a snapshot and never touches a socket.
- **Machine-local state lives outside Path1.** Pairing state, the pid file, and the workdir describe one machine and must not travel to another.
- **A packet that adds an export owns the whole export path.** `cloudsync.zig`, both `.def` files, the facade, and the ABI smoke test are in its allowlist. There is no packet whose exports another packet is expected to wire up later.
- Legacy C++ sources carry CP1251 comments. After editing one, restore clobbered comment lines from `git show HEAD:<file>` and keep CRLF.

## Stable file map

### Zig module

- `Sources/src/CloudSync/cloudsync.zig` — root module and C ABI exports.
- `Sources/src/CloudSync/rc.zig` — rc JSON client: request build, POST, parse, job polling.
- `Sources/src/CloudSync/daemon.zig` — rclone discovery, spawn, readiness, reap.
- `Sources/src/CloudSync/plan.zig` — short link, session-name budget, filters, sentinel, bisync parameters.
- `Sources/src/CloudSync/worker.zig` — the thread that owns every rc call, and the state snapshot `poll()` reads.
- `Sources/src/CloudSync/engine.zig` — pairing, sync state machine, error classification, trash pruning.
- `Sources/src/CloudSync/backup.zig` — config snapshot, listing, retention, restore merge.
- `Sources/src/CloudSync/creds.zig` — `profiles/cloud.credentials` read/write and remote parameter assembly.
- `Sources/src/CloudSync/CloudSync.def` / `CloudSync.x64.def` — Windows export lists.

### C++ integration

- `Sources/src/Main/CloudSyncFacade.h/.cpp` — `CloudSyncAvailable/Begin/Poll/Error/Restore` over the C ABI.
- `Sources/src/Game/GameMain.cpp` — startup pull before the profile config is read; exit push.
- `Sources/src/GameTT/InterfaceOptionsSettings.cpp/.h` — stale division constants, bounds guard, Cloud tab hooks.
- `Sources/src/GameTT/InterfaceCloudCredentials.cpp/.h` — the credentials dialog.

### Remote layout

```
<remote>/profiles/<name>/               Path2 — the only synced prefix
<remote>/trash/<name>/<run_id>/         backupDir2, one directory per run
<remote>/config-backups/<name>/<host>/  config snapshots, never synced
```

### Data

- `Data/Configs/defconf.cfg` — `Cloud.*` option declarations.
- `Data/Textes/Options/Cloud*.txt` — division and option labels.
- `Data/UI/CloudCredentials.xml` — the credentials dialog layout.

### Build and tests

- `build.zig` — `addCloudSync`, module wiring, test steps.
- `tools/zig/cloudsync_*_test.zig` — offline unit tests over a fake filesystem and a stub rc server.

## Phase graph

```text
00 rc transport foundation
        |
        v
01 sync planning primitives
        |
        v
02 sync engine  (+ ABI wiring)
        |
        +-------------------+-------------------+
        v                   v                   v
03 credentials       04 config backup     05 settings data
   and backends         and restore          (options, tab guard)
        |                   |                   |
        +-------------------+-------------------+
                            v
                 06 C++ integration and lifecycle
                            |
                            v
                 07 cloud UI (credentials, backups)
                            |
                            v
                 08 native acceptance
```

Everything Zig-side lands before any C++ consumer, and every option is
declared before a hook reads it. Phases 03, 04, and 05 are independent of one
another and may run in any order or in parallel.

## Phase completion contracts

| Phase | Required gate |
|---|---|
| 00 | rc client drives a live `rclone rcd` and survives a server that never replies; daemon spawns, reports ready, and is reaped with no orphan |
| 01 | Short link, session budget, filters, sentinel, and parameter builder pass offline unit tests |
| 02 | A full pair/diverge/converge cycle over two local directories passes, conflicts and both trashes included, driven end to end through the C ABI |
| 03 | S3-compatible and WebDAV backends pass the phase-02 cycle against a real remote, credentials never touching the option system |
| 04 | A config snapshot uploads per host, prunes to retention, and restores while preserving local `GFX.*`, all reachable through the ABI |
| 05 | The Cloud tab appears as the fifth division with all six options, and a synthetic seventh division does not crash the screen |
| 06 | The game starts, syncs, and exits with no main-thread socket call on any target |
| 07 | A player can enter credentials, test the connection, browse backups, restore one, and undo it |
| 08 | Two machines converge, including a delete and a conflict, on Windows and macOS |

## Packet index

- `phase-00-rc-transport`: P00-M01 through P00-M04
- `phase-01-planning-primitives`: P01-M01 through P01-M04
- `phase-02-sync-engine`: P02-M01 through P02-M05
- `phase-03-credentials-backends`: P03-M01 through P03-M04
- `phase-04-config-backup`: P04-M01 through P04-M04
- `phase-05-settings-data`: P05-M01 through P05-M02
- `phase-06-cpp-integration`: P06-M01 through P06-M04
- `phase-07-cloud-ui`: P07-M01 through P07-M03
- `phase-08-acceptance`: P08-M01 through P08-M04

This is 34 independently reviewable packets. Each packet has an explicit allowlist, a failing test, an implementation boundary, commands, evidence, and a commit. The coordinator starts the next packet only after the current packet is committed, pushed, and its gate passes.

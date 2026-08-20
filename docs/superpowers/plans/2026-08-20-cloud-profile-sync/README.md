# Cloud Profile Sync Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to implement exactly one packet at a time. Do not redesign the transport or combine packets. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Sync `profiles/<name>/` to a cloud provider by driving rclone's rc API over a localhost socket, with the provider chosen from a new Cloud tab in the settings screen, and `config.cfg` backed up per host rather than synced.

**Architecture:** A Zig module `Sources/src/CloudSync` speaks rclone's JSON rc API to a child `rclone rcd` process, and exports a five-function C ABI to C++ exactly as `StreamIOZig` does. `sync/bisync` is the diff engine; the plan never writes one. Path1 is a short game-managed link to the profile directory, path2 is a named remote. Credentials live outside the option system and outside `config.cfg`.

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

- **A sync never destroys a save.** Conflict losers survive as `.conflictN`; deletions and overwrites are diverted to the per-profile trash by `backupDir1`/`backupDir2`. A packet that can lose a save is wrong even if its tests pass.
- Path1 is always the short game-managed link, never the install path. The projected bisync session name is validated before every run.
- `maxDelete` is sent explicitly on every `sync/bisync` call. The rc API defaults it to 0, not to the CLI's 50.
- The `.bkprofile` sentinel is written at pairing and never rewritten. Two safety guards depend on it.
- `force: true` is never sent. It disables the excess-deletes guard.
- `config.cfg` is never in the sync set. It is backed up one-way and restored only on explicit request.
- Credentials never enter the option system, `config.cfg`, or any file that is synced or backed up.
- The game never blocks on a socket. Every rc call that can take time uses `_async` and is polled from the main loop.
- The rcd child binds 127.0.0.1 only, on a per-launch random port with a per-launch random user and password.
- Zig owns the sync logic; C++ owns only the facade, the hooks, and the UI.
- Legacy C++ sources carry CP1251 comments. After editing one, restore clobbered comment lines from `git show HEAD:<file>` and keep CRLF.

## Stable file map

### Zig module

- `Sources/src/CloudSync/cloudsync.zig` — root module and C ABI exports.
- `Sources/src/CloudSync/rc.zig` — rc JSON client: request build, POST, parse, job polling.
- `Sources/src/CloudSync/daemon.zig` — rclone discovery, spawn, readiness, reap.
- `Sources/src/CloudSync/plan.zig` — short link, session-name budget, filters, sentinel, bisync parameters.
- `Sources/src/CloudSync/engine.zig` — pairing, sync state machine, error classification, trash pruning.
- `Sources/src/CloudSync/backup.zig` — config snapshot, listing, retention, restore merge.
- `Sources/src/CloudSync/creds.zig` — `profiles/cloud.credentials` read/write and remote parameter assembly.
- `Sources/src/CloudSync/CloudSync.def` / `CloudSync.x64.def` — Windows export lists.

### C++ integration

- `Sources/src/Main/CloudSyncFacade.h/.cpp` — `CloudSyncAvailable/Begin/Poll/Error/Restore` over the C ABI.
- `Sources/src/Game/GameMain.cpp` — startup pull before the profile config is read; exit push.
- `Sources/src/GameTT/InterfaceOptionsSettings.cpp/.h` — stale division constants, bounds guard, Cloud tab hooks.
- `Sources/src/GameTT/InterfaceCloudCredentials.cpp/.h` — the credentials dialog.

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
02 sync engine
        |
        +---------------------------> 05 config backup and restore
        v
03 C++ integration and lifecycle
        |
        v
04 settings tab and credentials
        |
04 + 05 -----> 06 backends
        |
        v
07 native acceptance
```

## Phase completion contracts

| Phase | Required gate |
|---|---|
| 00 | rc client drives a live `rclone rcd`; daemon spawns, reports ready, and is reaped with no orphan |
| 01 | Short link, session budget, filters, sentinel, and parameter builder pass offline unit tests |
| 02 | A full pair/diverge/converge cycle over two local directories passes, conflicts and trash included |
| 03 | The game starts, syncs, and exits without blocking the main loop on any target |
| 04 | The Cloud tab appears as the fifth division and the credentials dialog round-trips a 40-character secret |
| 05 | A config snapshot uploads per host, prunes to retention, and restores while preserving local `GFX.*` |
| 06 | S3-compatible and WebDAV backends pass the phase-02 cycle against a real remote |
| 07 | Two machines converge, including a delete and a conflict, on Windows and macOS |

## Packet index

- `phase-00-rc-transport`: P00-M01 through P00-M04
- `phase-01-planning-primitives`: P01-M01 through P01-M04
- `phase-02-sync-engine`: P02-M01 through P02-M04
- `phase-03-cpp-integration`: P03-M01 through P03-M04
- `phase-04-settings-tab`: P04-M01 through P04-M04
- `phase-05-config-backup`: P05-M01 through P05-M04
- `phase-06-backends`: P06-M01 through P06-M03
- `phase-07-acceptance`: P07-M01 through P07-M04

This is 31 independently reviewable packets. Each packet has an explicit allowlist, a failing test, an implementation boundary, commands, evidence, and a commit. The coordinator starts the next packet only after the current packet is committed, pushed, and its gate passes.

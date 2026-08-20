# P05-M01 — config snapshot upload

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Push a copy of config.cfg after a successful sync, using none of bisync's machinery.

**Dependencies:** P02-M04.

**Allowed files:** `Sources/src/CloudSync/backup.zig`, `Sources/src/CloudSync/backup_test.zig`.

- [ ] Write the failing test against a live daemon and a local directory standing in for the remote.
- [ ] Implement `snapshotConfig(allocator, client, ctx) !void` issuing a single `operations/copyfile` with `srcFs`, `srcRemote`, `dstFs`, `dstRemote`. No listings, no session state, no delete ratios — none of bisync applies here.
- [ ] Name the destination `<remote>/profiles/<name>/config-backups/<host>/<timestamp>.cfg`, with the host sanitised through the same rules as `NProfile::Sanitize`.
- [ ] Key by host so machines never overwrite each other's history; "restore the desktop's settings onto the laptop" is only meaningful if both histories survive.
- [ ] Run it only after a clean sync finish, and only when `Cloud.Config.Backup` is enabled.
- [ ] Confirm `config.cfg` remains excluded from the sync filter set. It is backed up, never synced — machine-specific `GFX.Mode`, `GFX.Monitor`, and `GFX.FullScreen` are exactly what this split protects.
- [ ] Commit checkpoint: `cloudsync: snapshot config.cfg per host`.

**Evidence:** Integration output shows a snapshot at the expected per-host path and `config.cfg` still absent from the sync set.

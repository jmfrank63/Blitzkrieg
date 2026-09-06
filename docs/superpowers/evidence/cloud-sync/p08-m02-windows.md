# P08-M02 — Windows acceptance

Measured 2026-08-21 on Windows 11 Pro N 10.0.26100, release build
(`zig build install-game -Dtarget=x86_64-windows-msvc --release=fast`),
built natively on the Windows machine. Developer Mode **off**
(`HKLM\...\AppModelUnlock\AllowDevelopmentWithoutDevLicense` absent) and the
shell **unelevated** (`WindowsPrincipal.IsInRole(Administrator)` = False);
independently corroborated by `zig build`'s SDL3 `.so` symlink failing with
`PermissionDenied` on this machine — symlinks need privileges here,
junctions must not.

## The short link, wired in and exercised (a found gap, fixed here)

Running the deep-path item exposed that `plan.ensureShortLink` — built and
tested in P01 — had **no production caller**: `bisyncParams` measured the
session budget against the relative `path1` the facade sends
(`profiles/USSR`, ~30 bytes) while rclone canonicalises to the absolute
path (the P06-M02 workdir listings carried the full install path, ~110
bytes), and a deep install would have died on a 255-byte state filename
exactly as the design predicted. The worker now routes every
transfer-shaped job (pair, sync, restore download) through
`ensureShortLinkIn` before the session budget is checked, so the projection
measures the exact bytes rclone mangles and Path1 is constant across
installs; on a machine where the link cannot be made it falls back to the
raw path with the budget check still guarding. `Worker.Options.link_roots`
lets tests inject a fixture root. Changing Path1 changes the session name,
so existing pairings re-pair once (pre-release, acceptable).

## Junction creation in the shipped build

- `%LOCALAPPDATA%\bk\p0 → <release install>\profiles\USSR` was created by
  the shipped `CloudSync.dll` (the `FSCTL_SET_REPARSE_POINT` path) during a
  normal unelevated launch with Developer Mode off, and the launch paired:
  `cloud sync: sync finished (paired)`.
- Session state files after the re-pair:
  `C__Users_jmfrank_AppData_Local_bk_p0..bkraw_bk-saves_profiles_USSR.path1.lst`
  (76 bytes with suffix) — the junction root **unresolved** in the session
  name, re-confirming the P01 probe in the shipped build.
- Data moved through the junction both ways: the profile's eight real
  saves (`quick.sav`, autosaves, named saves) verified present in the
  bucket via `rclone lsf` after the pair, and a second launch settled
  `(synced)`.

## Deep install path

- Steam-style path staged with a full 2.9 GB copy of the install:
  `%TEMP%\SteamLibrary\steamapps\common\Blitzkrieg Anthology Complete
  Edition Remastered Deluxe\Blitzkrieg 1 - The Original Campaigns Gold
  Edition Bonus Content Directory` — install path 196 chars, profile
  directory 210 chars, projected raw session name ~241 of the 241-byte
  budget (the edge; the original probe's Steam path measured 212).
- The launch from the deep path paired cleanly. The link machinery
  allocated slot `p1` for the deep profile while `p0` kept the release
  install's — two installs of the same profile coexist — and the deep
  install's session name is byte-for-byte the same shape and length as the
  shallow one: `C__Users_jmfrank_AppData_Local_bk_p1..bkraw_bk-saves_
  profiles_USSR.path1.lst`, 76 bytes. Install depth no longer reaches the
  budget at all.

## Daemon reaping

- **Job object:** with the game running, exactly one `rclone` process was
  alive and `cloudsync/daemon.json` present. `Stop-Process -Force` on
  `Game.exe` (no shutdown path of ours runs) left **zero** rclone
  processes three seconds later — `KILL_ON_JOB_CLOSE` reaped it — with the
  stale `daemon.json` left behind for the next launch, as designed.
- **Recycled pid declined:** `daemon.json` was rewritten to name the pid
  of a live decoy process (a sleeping PowerShell) with a wrong
  `process_start_time`. The next launch declined it — the decoy survived
  the whole run — and the game spawned its own daemon and synced
  normally.

## Suites

`zig build test` green offline and live (BK_TEST_RCLONE/BK_TEST_MINIO);
`test-cloudsync-worker` and `test-cloudsync-facade` cross-compile for
`x86_64-linux-gnu`, `aarch64-linux-gnu`, `x86_64-windows-gnu`.

## Human playability approval

**Pending.** The packet requires a human to play the shipped build and
approve; everything above is machine-verified. Approval to be recorded
here when given.

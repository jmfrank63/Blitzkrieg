# Phase 00 — Bundled rclone

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Ship the binary so the feature works out of the box.

| Packet | Depends on | Owns |
|---|---|---|
| P00-M01 | — | hashed build dependency and staging |
| P00-M02 | M01 | availability out of the box, no PATH |
| P00-M03 | M02 | packaging, size and third-party notices |
| P00-M04 | M03 | package permissions and handle exhaustion |

Exit: a fresh install on a machine with no rclone reports cloud sync available.

P00-M01 macOS checkpoint: `zig build install-game -Dtarget=aarch64-macos
--release=fast` stages an executable `rclone` reporting v1.75.0, and it runs
with `PATH` emptied. All five CloudSync suites and `test-streamio` still pass.
Commit `319602e62`.

Sizes fetched / installed: macos-arm64 32.5/88.4 MB, macos-x64 34.9/95.5,
linux-x64 31.5/85.3, linux-arm64 28.6/78.3, windows-x64 31.5/85.2.

Two findings for later packets:

- **Zig's zip extractor ignores the archive's external file attributes**, so the
  extracted `rclone` is `-rw-rw-r--` in the package cache, and both
  `Step.installFile` and `stage.zig`'s copy preserve their *source's*
  permissions. The executable bit would therefore never have appeared and
  discovery would have rejected the staged file as `.not_executable`. It is set
  once on the way into `zig-out/bin` by a cached `Step.Run` of
  `install -m 0755`, and staging carries it from there. That step is a new
  host-tool dependency on POSIX; it is skipped on Windows hosts.
- **`lazy` is conditional on the target, not on the step.** Only the archive for
  `-Dtarget` is ever requested, which is what the packet asked for. But the Zig
  0.16 build runner resolves lazy dependencies during *configure*, before it
  selects steps, so on a cold cache even `zig build test-streamio` pulls that
  target's archive. There is no supported way for `build()` to see which step
  was requested. The packet's "a build that never stages the game does not
  fetch 31 MB" is therefore only partly true in practice.

Not a defect in this packet: `install-game -Dtarget=x86_64-linux-gnu` fails
compiling the engine's C++ (`'stdio.h' file not found`) — cross-compiling to
Linux from this macOS host has no libc headers. Reproduced on the unchanged
tree by stashing. The Linux rclone itself fetched and installed correctly
before that failure.

P00-M02 macOS checkpoint: 27/27 daemon tests and 10/10 ABI tests plus the C++
consumer pass, with and without a real rclone available; `rc`, `plan` and
`streamio` unaffected; `x86_64-linux-gnu` and `x86_64-windows-gnu` compile.
Commit `a70a3dab9`. Tests only — `daemon.zig` was not touched, which is the
right outcome: P00-M01 already made discovery search the executable's directory.

The mutation check is what makes these tests worth having. Removing the staging
produced three Zig failures and four C++ failures; mutating one assertion
inside the `BK_TEST_RCLONE` branch failed only that line, proving the
real-binary branch runs rather than silently skipping.

How the fixtures avoid depending on the build output:

- Zig injects a `TmpDir` as `Search.game_dir` with `path_env = ""`, so no path
  under `zig-out` appears. Stubs are `/bin/sh` scripts printing chosen banners.
- C++ has no `game_dir` injection — `bk_cloudsync_available` uses
  `std.process.executableDirPath` — so the consumer finds its own directory
  (`_NSGetExecutablePath`, `/proc/self/exe`, `GetModuleFileNameA`) and stages
  `rclone` beside itself, which is exactly the shipped neighbour relationship.
  It refuses to overwrite a pre-existing neighbour and restores `PATH`.
- Identity is proved by version rather than by path string: the stubs print
  `v9.75.3` and `v9.66.1`, versions no real rclone has printed, so seeing one
  in `discovery_status` proves which binary answered.

Windows remains compile-verified only: the C++ stub cases are `#ifndef _WIN32`,
so there the function runs only its real-binary branch and only when
`BK_TEST_RCLONE` is set.

P00-M03 macOS checkpoint: the layout stages `rclone` (exec bit intact) and
`THIRD-PARTY-NOTICES.txt` at its root; `verify-runtime` 11/11 now requires
both, `test-cloudsync-daemon` 27/27 and `test-streamio` 32/32 unaffected. Three
independent stagings produced an identical package hash
`cbd57eb6c7c7baa7d274bb8b7331f1e6ad4a0f29713a09e677bc1a1596fe1d45`, so
determinism holds. Commit `86c53c970`.

The notice is a file we own, not a build-time scrape: the archive carries no
`COPYING`, only `rclone`, `rclone.1`, `README.html`, `README.txt` and
`git-log.txt`, with the MIT text buried in the README. It was copied verbatim
and diffed byte-for-byte against the source. It stages at the layout **root**
rather than under `Data/`, beside `LICENSE.md`, because `--link-data` replaces
staged `Data` with a link into the repo and would take the notice with it.

Signing was not performed and no signature is asserted, per the packet.

Two defects found in `tools/zig/package.zig`, which is outside this packet's
allowlist and is why `P00-M04` now exists:

- **The zip drops the executable bit.** It writes no external file attributes
  at all, so `0755` in becomes `0644` out. Pre-existing and equally true of
  `Game` and every dylib, but for the bundled rclone it means a player who
  installs from the release zip gets `.not_executable` at discovery — this
  feature would ship broken.
- **`package-game` cannot complete on this host**: the zip walk opens every
  entry and holds the handle to the end, and 63,728 files exceeds
  `kern.maxfilesperproc` (61,440), so it fails `ProcessFdQuotaExceeded` and
  leaves a zero-byte archive. No `ulimit` raise helps. The hashes above came
  from an out-of-tree copy differing only in when each file is opened.

Also fixed here, pre-existing: the package stage directory was `<root>/game`,
which on a case-insensitive filesystem collides with the staged `Game`
executable — `package-game` had never worked on macOS. Renamed to `package`.

P00-M04 macOS checkpoint: `package-game` now **completes** — 26 s, 2,876,018,942
bytes, 63,728 entries — where it previously died `ProcessFdQuotaExceeded` and
left a zero-byte archive. The walk holds one handle at a time. Permissions
survive: exactly 17 entries are `-rwxr-xr-x` (`Game`, fifteen dylibs, `rclone`)
and the other 63,711 stay `0644`, taken per source file rather than hardcoded.
The extracted `rclone` runs with `PATH` emptied and reports v1.75.0. Three runs
produced one hash, `52790811536b03191d40eb56a4a67a23cb6044a0773d3f22f5c05f3490d87b0d`.
`test-package` 4/4, `verify-runtime` 11/11, daemon 27/27, streamio 32/32.
Commit `6dccbc024`.

The hash necessarily differs from P00-M03's `cbd57eb6…`: version-made-by and
external attributes now differ in all 63,728 central-directory records. The
1980-00-00 stamp and name ordering are untouched, so determinism is intact.

A partial archive is now written as `.partial` and renamed only after the final
flush, so a failure leaves nothing that looks like a product.

**Carried forward, not fixed here: the layout is close to zip's hard ceiling.**
63,728 entries against 65,535, and 2.88 GB against 4 GiB. The writer previously
`@intCast` the entry count, sizes and offsets, so in ReleaseFast an overrun
would have silently truncated into an archive that opens and lies; those casts
now fail loudly as `TooManyEntries` / `ArchiveTooLarge`. Zip64 is the real fix
and is not done — roughly 1,800 more files reaches the cliff.

Windows and Linux packaging remain unverified; only `aarch64-macos` was
exercised, since `-Dtest-mode=run` cannot cross.

